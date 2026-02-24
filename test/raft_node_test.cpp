#include "raft/raft_node.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace raft;

// 测试辅助函数
void assertTrue(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        exit(1);
    }
    std::cout << "PASSED: " << message << std::endl;
}

void testInitialState() {
    std::cout << "\n=== Test: Initial State ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    assertTrue(node.getState() == NodeState::FOLLOWER, 
               "Node should start as FOLLOWER");
    assertTrue(node.getCurrentTerm() == 0, 
               "Initial term should be 0");
    assertTrue(node.getLeaderId() == -1, 
               "Initial leader ID should be -1");
    assertTrue(!node.isLeader(), 
               "Node should not be leader initially");
    assertTrue(node.getLogSize() == 0, 
               "Initial log size should be 0 (excluding sentinel)");
}

void testStateTransitions() {
    std::cout << "\n=== Test: State Transitions ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // Test: Follower -> Candidate
    node.startElection();
    assertTrue(node.getState() == NodeState::CANDIDATE, 
               "Node should become CANDIDATE after startElection");
    assertTrue(node.getCurrentTerm() == 1, 
               "Term should increment after startElection");
    
    // Test: Candidate -> Leader
    node.becomeLeader();
    assertTrue(node.getState() == NodeState::LEADER, 
               "Node should become LEADER");
    assertTrue(node.isLeader(), 
               "isLeader() should return true");
    assertTrue(node.getLeaderId() == 1, 
               "Leader ID should be set to node ID");
    
    // Test: Leader -> Follower
    node.becomeFollower(2);
    assertTrue(node.getState() == NodeState::FOLLOWER, 
               "Node should become FOLLOWER");
    assertTrue(node.getCurrentTerm() == 2, 
               "Term should be updated");
    assertTrue(!node.isLeader(), 
               "isLeader() should return false");
}

void testRequestVote() {
    std::cout << "\n=== Test: RequestVote RPC ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // Test 1: Vote for candidate with up-to-date log
    RequestVoteArgs args1(1, 2, 0, 0);
    RequestVoteReply reply1;
    node.handleRequestVote(args1, reply1);
    
    assertTrue(reply1.voteGranted, 
               "Should grant vote to candidate with up-to-date log");
    assertTrue(reply1.term == 1, 
               "Reply term should match request term");
    
    // Test 2: Reject vote for same term (already voted)
    RequestVoteArgs args2(1, 3, 0, 0);
    RequestVoteReply reply2;
    node.handleRequestVote(args2, reply2);
    
    assertTrue(!reply2.voteGranted, 
               "Should reject vote (already voted in this term)");
    
    // Test 3: Reject vote for lower term
    RequestVoteArgs args3(0, 2, 0, 0);
    RequestVoteReply reply3;
    node.handleRequestVote(args3, reply3);
    
    assertTrue(!reply3.voteGranted, 
               "Should reject vote for lower term");
    assertTrue(reply3.term == 1, 
               "Reply should include current term");
    
    // Test 4: Accept vote for higher term
    RequestVoteArgs args4(2, 3, 0, 0);
    RequestVoteReply reply4;
    node.handleRequestVote(args4, reply4);
    
    assertTrue(reply4.voteGranted, 
               "Should grant vote for higher term");
    assertTrue(node.getCurrentTerm() == 2, 
               "Should update to higher term");
    assertTrue(node.getState() == NodeState::FOLLOWER, 
               "Should become follower when seeing higher term");
}

void testAppendEntries() {
    std::cout << "\n=== Test: AppendEntries RPC ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // Test 1: Accept heartbeat from leader
    AppendEntriesArgs args1(1, 2, 0, 0, {}, 0);
    AppendEntriesReply reply1;
    node.handleAppendEntries(args1, reply1);
    
    assertTrue(reply1.success, 
               "Should accept heartbeat from leader");
    assertTrue(node.getCurrentTerm() == 1, 
               "Should update term");
    assertTrue(node.getLeaderId() == 2, 
               "Should recognize leader");
    
    // Test 2: Reject AppendEntries from lower term
    AppendEntriesArgs args2(0, 2, 0, 0, {}, 0);
    AppendEntriesReply reply2;
    node.handleAppendEntries(args2, reply2);
    
    assertTrue(!reply2.success, 
               "Should reject AppendEntries from lower term");
    
    // Test 3: Accept log entries
    std::vector<LogEntry> entries;
    entries.push_back(LogEntry(1, "PUT:key1:value1", 1));
    AppendEntriesArgs args3(1, 2, 0, 0, entries, 0);
    AppendEntriesReply reply3;
    node.handleAppendEntries(args3, reply3);
    
    assertTrue(reply3.success, 
               "Should accept log entries");
    assertTrue(node.getLogSize() == 1, 
               "Log size should be 1 after appending entry");
    
    // Test 4: Reject if prevLogIndex doesn't match
    std::vector<LogEntry> entries2;
    entries2.push_back(LogEntry(1, "PUT:key2:value2", 3));
    AppendEntriesArgs args4(1, 2, 2, 1, entries2, 0);
    AppendEntriesReply reply4;
    node.handleAppendEntries(args4, reply4);
    
    assertTrue(!reply4.success, 
               "Should reject if prevLogIndex doesn't exist");
    assertTrue(reply4.conflictIndex > 0, 
               "Should provide conflict index for optimization");
}

void testLogOperations() {
    std::cout << "\n=== Test: Log Operations ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // Only leader can append logs
    int index1 = node.appendLogEntry("PUT:key1:value1");
    assertTrue(index1 == -1, 
               "Follower should not be able to append log");
    
    // Become leader
    node.startElection();
    node.becomeLeader();
    
    // Append logs as leader
    int index2 = node.appendLogEntry("PUT:key1:value1");
    assertTrue(index2 == 1, 
               "First log entry should have index 1");
    
    int index3 = node.appendLogEntry("PUT:key2:value2");
    assertTrue(index3 == 2, 
               "Second log entry should have index 2");
    
    assertTrue(node.getLogSize() == 2, 
               "Log size should be 2");
    assertTrue(node.getLastLogIndex() == 2, 
               "Last log index should be 2");
    assertTrue(node.getLastLogTerm() == 1, 
               "Last log term should be 1");
    
    // Get log entry
    LogEntry entry = node.getLogEntry(1);
    assertTrue(entry.index == 1, 
               "Retrieved entry should have correct index");
    assertTrue(entry.command == "PUT:key1:value1", 
               "Retrieved entry should have correct command");
}

void testElectionTimeout() {
    std::cout << "\n=== Test: Election Timeout ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // Initially should not timeout
    assertTrue(!node.checkElectionTimeout(), 
               "Should not timeout immediately");
    
    // Wait for election timeout (max 300ms + buffer)
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    
    assertTrue(node.checkElectionTimeout(), 
               "Should timeout after election timeout period");
    
    // Reset timer
    node.resetElectionTimer();
    assertTrue(!node.checkElectionTimeout(), 
               "Should not timeout after reset");
}

void testHeartbeatTimer() {
    std::cout << "\n=== Test: Heartbeat Timer ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // Follower should not send heartbeat
    assertTrue(!node.shouldSendHeartbeat(), 
               "Follower should not send heartbeat");
    
    // Become leader
    node.startElection();
    node.becomeLeader();
    
    // Initially should not need to send heartbeat (just became leader)
    assertTrue(!node.shouldSendHeartbeat(), 
               "Should not need heartbeat immediately after becoming leader");
    
    // Wait for heartbeat interval (50ms + buffer)
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    
    assertTrue(node.shouldSendHeartbeat(), 
               "Should need to send heartbeat after interval");
    
    // Reset timer
    node.resetHeartbeatTimer();
    assertTrue(!node.shouldSendHeartbeat(), 
               "Should not need heartbeat after reset");
}

void testConcurrentAccess() {
    std::cout << "\n=== Test: Concurrent Access ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // Become leader
    node.startElection();
    node.becomeLeader();
    
    // Spawn multiple threads to append logs concurrently
    const int numThreads = 10;
    const int logsPerThread = 10;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&node, i, logsPerThread]() {
            for (int j = 0; j < logsPerThread; ++j) {
                std::string command = "PUT:key" + std::to_string(i * logsPerThread + j) + ":value";
                node.appendLogEntry(command);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    assertTrue(node.getLogSize() == numThreads * logsPerThread, 
               "All log entries should be appended correctly");
    
    std::cout << "Successfully appended " << node.getLogSize() 
              << " log entries concurrently" << std::endl;
}

void testLeaderElectionLogic() {
    std::cout << "\n=== Test: Leader Election Logic ===" << std::endl;
    
    std::vector<int> peers = {2, 3, 4, 5};  // 5 node cluster
    RaftNode node(1, peers);
    
    // Test initial vote count
    assertTrue(node.getVoteCount() == 0, 
               "Initial vote count should be 0");
    assertTrue(node.getMajorityThreshold() == 3, 
               "Majority threshold for 5 nodes should be 3");
    
    // Start election
    node.startElection();
    assertTrue(node.getState() == NodeState::CANDIDATE, 
               "Node should be CANDIDATE after startElection");
    assertTrue(node.getVoteCount() == 1, 
               "Vote count should be 1 (self vote) after startElection");
    
    // Simulate receiving votes
    RequestVoteReply reply1(1, true);   // Vote granted
    RequestVoteReply reply2(1, false);  // Vote denied
    RequestVoteReply reply3(1, true);   // Vote granted
    
    // Handle first vote (not enough for majority)
    bool becameLeader = node.handleVoteResponse(2, reply1);
    assertTrue(!becameLeader, 
               "Should not become leader with 2 votes out of 5");
    assertTrue(node.getVoteCount() == 2, 
               "Vote count should be 2 after first granted vote");
    assertTrue(node.getState() == NodeState::CANDIDATE, 
               "Should remain CANDIDATE");
    
    // Handle denied vote
    becameLeader = node.handleVoteResponse(3, reply2);
    assertTrue(!becameLeader, 
               "Should not become leader after denied vote");
    assertTrue(node.getVoteCount() == 2, 
               "Vote count should remain 2 after denied vote");
    
    // Handle third vote (should reach majority)
    becameLeader = node.handleVoteResponse(4, reply3);
    assertTrue(becameLeader, 
               "Should become leader with 3 votes out of 5");
    assertTrue(node.getState() == NodeState::LEADER, 
               "Should be LEADER after reaching majority");
    assertTrue(node.isLeader(), 
               "isLeader() should return true");
    assertTrue(node.getVoteCount() == 0, 
               "Vote count should be cleared after becoming leader");
}

void testSplitVoteScenario() {
    std::cout << "\n=== Test: Split Vote Scenario ===" << std::endl;
    
    std::vector<int> peers = {2, 3};  // 3 node cluster
    RaftNode node(1, peers);
    
    assertTrue(node.getMajorityThreshold() == 2, 
               "Majority threshold for 3 nodes should be 2");
    
    // Start election
    node.startElection();
    int initialTerm = node.getCurrentTerm();
    
    // Simulate split vote (only get 1 vote, need 2 for majority)
    RequestVoteReply reply1(initialTerm, false);  // Vote denied
    RequestVoteReply reply2(initialTerm, false);  // Vote denied
    
    bool becameLeader1 = node.handleVoteResponse(2, reply1);
    bool becameLeader2 = node.handleVoteResponse(3, reply2);
    
    assertTrue(!becameLeader1 && !becameLeader2, 
               "Should not become leader in split vote");
    assertTrue(node.getState() == NodeState::CANDIDATE, 
               "Should remain CANDIDATE after split vote");
    assertTrue(node.getVoteCount() == 1, 
               "Should only have self vote after split vote");
    
    // Simulate election timeout and restart
    node.startElection();
    assertTrue(node.getCurrentTerm() == initialTerm + 1, 
               "Term should increment on new election");
    assertTrue(node.getVoteCount() == 1, 
               "Should reset to self vote on new election");
}

void testHigherTermResponse() {
    std::cout << "\n=== Test: Higher Term Response ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // Start election (term becomes 1)
    node.startElection();
    assertTrue(node.getCurrentTerm() == 1, 
               "Term should be 1 after startElection");
    assertTrue(node.getState() == NodeState::CANDIDATE, 
               "Should be CANDIDATE");
    
    // Receive vote response with higher term
    RequestVoteReply reply(2, false);  // Higher term, vote denied
    bool becameLeader = node.handleVoteResponse(2, reply);
    
    assertTrue(!becameLeader, 
               "Should not become leader when seeing higher term");
    assertTrue(node.getCurrentTerm() == 2, 
               "Should update to higher term");
    assertTrue(node.getState() == NodeState::FOLLOWER, 
               "Should become FOLLOWER when seeing higher term");
    assertTrue(node.getVoteCount() == 0, 
               "Vote count should be cleared when becoming follower");
}

void testOutdatedVoteResponse() {
    std::cout << "\n=== Test: Outdated Vote Response ===" << std::endl;
    
    std::vector<int> peers = {2, 3};
    RaftNode node(1, peers);
    
    // Start first election (term becomes 1)
    node.startElection();
    int firstTerm = node.getCurrentTerm();
    
    // Start second election (term becomes 2)
    node.startElection();
    int secondTerm = node.getCurrentTerm();
    assertTrue(secondTerm == firstTerm + 1, 
               "Second election should increment term");
    
    // Receive outdated vote response from first election
    RequestVoteReply outdatedReply(firstTerm, true);
    bool becameLeader = node.handleVoteResponse(2, outdatedReply);
    
    assertTrue(!becameLeader, 
               "Should ignore outdated vote response");
    assertTrue(node.getVoteCount() == 1, 
               "Vote count should not change for outdated response");
    assertTrue(node.getState() == NodeState::CANDIDATE, 
               "Should remain CANDIDATE");
    
    // Receive current vote response
    RequestVoteReply currentReply(secondTerm, true);
    becameLeader = node.handleVoteResponse(3, currentReply);
    
    assertTrue(becameLeader, 
               "Should become leader with current vote response");
    assertTrue(node.getState() == NodeState::LEADER, 
               "Should be LEADER");
}

int main() {
    std::cout << "Running RaftNode Tests..." << std::endl;
    
    try {
        testInitialState();
        testStateTransitions();
        testRequestVote();
        testAppendEntries();
        testLogOperations();
        testElectionTimeout();
        testHeartbeatTimer();
        testConcurrentAccess();
        testLeaderElectionLogic();
        testSplitVoteScenario();
        testHigherTermResponse();
        testOutdatedVoteResponse();
        
        std::cout << "\n=== All Tests Passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
