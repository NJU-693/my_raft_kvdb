#include "../include/raft/rpc_messages.h"
#include <cassert>
#include <iostream>

using namespace raft;

// ==================== RequestVote Tests ====================

void test_request_vote_args_construction() {
    std::cout << "Testing RequestVoteArgs construction..." << std::endl;
    
    RequestVoteArgs args1;
    assert(args1.term == 0);
    assert(args1.candidateId == 0);
    assert(args1.lastLogIndex == 0);
    assert(args1.lastLogTerm == 0);
    
    RequestVoteArgs args2(5, 2, 10, 4);
    assert(args2.term == 5);
    assert(args2.candidateId == 2);
    assert(args2.lastLogIndex == 10);
    assert(args2.lastLogTerm == 4);
    
    std::cout << "✓ RequestVoteArgs construction test passed" << std::endl;
}

void test_request_vote_args_serialization() {
    std::cout << "Testing RequestVoteArgs serialization..." << std::endl;
    
    RequestVoteArgs args(3, 1, 7, 2);
    std::string serialized = args.serialize();
    
    assert(serialized == "3|1|7|2");
    
    std::cout << "✓ RequestVoteArgs serialization test passed" << std::endl;
}

void test_request_vote_args_deserialization() {
    std::cout << "Testing RequestVoteArgs deserialization..." << std::endl;
    
    std::string serialized = "6|3|15|5";
    RequestVoteArgs args = RequestVoteArgs::deserialize(serialized);
    
    assert(args.term == 6);
    assert(args.candidateId == 3);
    assert(args.lastLogIndex == 15);
    assert(args.lastLogTerm == 5);
    
    std::cout << "✓ RequestVoteArgs deserialization test passed" << std::endl;
}

void test_request_vote_reply_construction() {
    std::cout << "Testing RequestVoteReply construction..." << std::endl;
    
    RequestVoteReply reply1;
    assert(reply1.term == 0);
    assert(reply1.voteGranted == false);
    
    RequestVoteReply reply2(5, true);
    assert(reply2.term == 5);
    assert(reply2.voteGranted == true);
    
    std::cout << "✓ RequestVoteReply construction test passed" << std::endl;
}

void test_request_vote_reply_serialization() {
    std::cout << "Testing RequestVoteReply serialization..." << std::endl;
    
    RequestVoteReply reply1(3, true);
    assert(reply1.serialize() == "3|1");
    
    RequestVoteReply reply2(4, false);
    assert(reply2.serialize() == "4|0");
    
    std::cout << "✓ RequestVoteReply serialization test passed" << std::endl;
}

void test_request_vote_reply_deserialization() {
    std::cout << "Testing RequestVoteReply deserialization..." << std::endl;
    
    RequestVoteReply reply1 = RequestVoteReply::deserialize("7|1");
    assert(reply1.term == 7);
    assert(reply1.voteGranted == true);
    
    RequestVoteReply reply2 = RequestVoteReply::deserialize("8|0");
    assert(reply2.term == 8);
    assert(reply2.voteGranted == false);
    
    std::cout << "✓ RequestVoteReply deserialization test passed" << std::endl;
}

// ==================== AppendEntries Tests ====================

void test_append_entries_args_construction() {
    std::cout << "Testing AppendEntriesArgs construction..." << std::endl;
    
    AppendEntriesArgs args1;
    assert(args1.term == 0);
    assert(args1.leaderId == 0);
    assert(args1.prevLogIndex == 0);
    assert(args1.prevLogTerm == 0);
    assert(args1.entries.empty());
    assert(args1.leaderCommit == 0);
    
    std::vector<LogEntry> entries = {
        LogEntry(1, "PUT:a:1", 1),
        LogEntry(1, "PUT:b:2", 2)
    };
    AppendEntriesArgs args2(1, 0, 0, 0, entries, 0);
    assert(args2.term == 1);
    assert(args2.leaderId == 0);
    assert(args2.entries.size() == 2);
    
    std::cout << "✓ AppendEntriesArgs construction test passed" << std::endl;
}

void test_append_entries_args_heartbeat() {
    std::cout << "Testing AppendEntriesArgs heartbeat (empty entries)..." << std::endl;
    
    // 心跳消息（空日志条目）
    AppendEntriesArgs heartbeat(5, 1, 10, 4, {}, 8);
    std::string serialized = heartbeat.serialize();
    
    AppendEntriesArgs deserialized = AppendEntriesArgs::deserialize(serialized);
    assert(deserialized.term == 5);
    assert(deserialized.leaderId == 1);
    assert(deserialized.prevLogIndex == 10);
    assert(deserialized.prevLogTerm == 4);
    assert(deserialized.entries.empty());
    assert(deserialized.leaderCommit == 8);
    
    std::cout << "✓ AppendEntriesArgs heartbeat test passed" << std::endl;
}

void test_append_entries_args_with_entries() {
    std::cout << "Testing AppendEntriesArgs with log entries..." << std::endl;
    
    std::vector<LogEntry> entries = {
        LogEntry(2, "PUT:key1:value1", 5),
        LogEntry(2, "DELETE:key2", 6),
        LogEntry(3, "GET:key3", 7)
    };
    
    AppendEntriesArgs args(3, 0, 4, 2, entries, 4);
    std::string serialized = args.serialize();
    
    AppendEntriesArgs deserialized = AppendEntriesArgs::deserialize(serialized);
    assert(deserialized.term == 3);
    assert(deserialized.leaderId == 0);
    assert(deserialized.prevLogIndex == 4);
    assert(deserialized.prevLogTerm == 2);
    assert(deserialized.leaderCommit == 4);
    assert(deserialized.entries.size() == 3);
    
    assert(deserialized.entries[0] == entries[0]);
    assert(deserialized.entries[1] == entries[1]);
    assert(deserialized.entries[2] == entries[2]);
    
    std::cout << "✓ AppendEntriesArgs with entries test passed" << std::endl;
}

void test_append_entries_reply_construction() {
    std::cout << "Testing AppendEntriesReply construction..." << std::endl;
    
    AppendEntriesReply reply1;
    assert(reply1.term == 0);
    assert(reply1.success == false);
    assert(reply1.conflictIndex == -1);
    assert(reply1.conflictTerm == -1);
    
    AppendEntriesReply reply2(5, true);
    assert(reply2.term == 5);
    assert(reply2.success == true);
    
    AppendEntriesReply reply3(6, false, 10, 3);
    assert(reply3.term == 6);
    assert(reply3.success == false);
    assert(reply3.conflictIndex == 10);
    assert(reply3.conflictTerm == 3);
    
    std::cout << "✓ AppendEntriesReply construction test passed" << std::endl;
}

void test_append_entries_reply_serialization() {
    std::cout << "Testing AppendEntriesReply serialization..." << std::endl;
    
    AppendEntriesReply reply1(5, true);
    assert(reply1.serialize() == "5|1|-1|-1");
    
    AppendEntriesReply reply2(6, false, 8, 4);
    assert(reply2.serialize() == "6|0|8|4");
    
    std::cout << "✓ AppendEntriesReply serialization test passed" << std::endl;
}

void test_append_entries_reply_deserialization() {
    std::cout << "Testing AppendEntriesReply deserialization..." << std::endl;
    
    AppendEntriesReply reply1 = AppendEntriesReply::deserialize("7|1|-1|-1");
    assert(reply1.term == 7);
    assert(reply1.success == true);
    assert(reply1.conflictIndex == -1);
    assert(reply1.conflictTerm == -1);
    
    AppendEntriesReply reply2 = AppendEntriesReply::deserialize("8|0|12|5");
    assert(reply2.term == 8);
    assert(reply2.success == false);
    assert(reply2.conflictIndex == 12);
    assert(reply2.conflictTerm == 5);
    
    std::cout << "✓ AppendEntriesReply deserialization test passed" << std::endl;
}

// ==================== Error Handling Tests ====================

void test_invalid_formats() {
    std::cout << "Testing invalid format handling..." << std::endl;
    
    bool caught = false;
    
    // Invalid RequestVoteArgs
    try {
        RequestVoteArgs::deserialize("1|2|3");  // 缺少字段
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);
    
    // Invalid RequestVoteReply
    caught = false;
    try {
        RequestVoteReply::deserialize("5");  // 缺少字段
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);
    
    // Invalid AppendEntriesArgs
    caught = false;
    try {
        AppendEntriesArgs::deserialize("1|2|3");  // 缺少字段
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);
    
    // Invalid AppendEntriesReply
    caught = false;
    try {
        AppendEntriesReply::deserialize("1|2");  // 缺少字段
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);
    
    std::cout << "✓ Invalid format handling test passed" << std::endl;
}

int main() {
    std::cout << "Running RPC Messages tests..." << std::endl;
    std::cout << "================================" << std::endl;
    
    // RequestVote tests
    test_request_vote_args_construction();
    test_request_vote_args_serialization();
    test_request_vote_args_deserialization();
    test_request_vote_reply_construction();
    test_request_vote_reply_serialization();
    test_request_vote_reply_deserialization();
    
    // AppendEntries tests
    test_append_entries_args_construction();
    test_append_entries_args_heartbeat();
    test_append_entries_args_with_entries();
    test_append_entries_reply_construction();
    test_append_entries_reply_serialization();
    test_append_entries_reply_deserialization();
    
    // Error handling
    test_invalid_formats();
    
    std::cout << "================================" << std::endl;
    std::cout << "All RPC Messages tests passed!" << std::endl;
    
    return 0;
}
