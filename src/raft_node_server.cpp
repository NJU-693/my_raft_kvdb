#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include "raft/raft_node.h"
#include "storage/kv_store.h"
#include "storage/persister.h"
#include "network/rpc_server.h"
#include "network/rpc_client.h"
#include "util/config.h"

// 全局变量用于优雅关闭
std::atomic<bool> g_running(true);
std::unique_ptr<network::RaftRPCServer> g_server;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running = false;
    if (g_server) {
        g_server->stop();
    }
}

// 辅助函数：向所有对等节点发送 RequestVote RPC
void sendRequestVotesToPeers(
    raft::RaftNode* raft_node,
    int node_id,
    const std::map<int, std::string>& peer_addresses) {
    
    if (raft_node->getState() != raft::NodeState::CANDIDATE) {
        return;  // 只有 Candidate 才发送投票请求
    }
    
    int term = raft_node->getCurrentTerm();
    int lastLogIndex = raft_node->getLastLogIndex();
    int lastLogTerm = raft_node->getLastLogTerm();
    
    // 向每个对等节点发送 RequestVote RPC
    for (const auto& [peer_id, address] : peer_addresses) {
        // 在新线程中发送 RPC，避免阻塞主循环
        std::thread([raft_node, peer_id, address, term, node_id, lastLogIndex, lastLogTerm]() {
            auto client = network::createRPCClient(address);
            if (client->connect(address)) {
                raft::RequestVoteArgs args(term, node_id, lastLogIndex, lastLogTerm);
                raft::RequestVoteReply reply;
                
                if (client->sendRequestVote(args, reply, 1000)) {
                    // 处理投票响应
                    raft_node->handleVoteResponse(peer_id, reply);
                }
                client->disconnect();
            }
        }).detach();
    }
}

// 辅助函数：向所有对等节点发送 AppendEntries RPC（心跳或日志复制）
void sendAppendEntriesToPeers(
    raft::RaftNode* raft_node,
    const std::vector<int>& peer_ids,
    const std::map<int, std::string>& peer_addresses) {
    
    if (!raft_node->isLeader()) {
        return;  // 只有 Leader 才发送 AppendEntries
    }
    
    // 向每个对等节点发送 AppendEntries RPC
    for (size_t i = 0; i < peer_ids.size(); ++i) {
        int peer_id = peer_ids[i];
        auto it = peer_addresses.find(peer_id);
        if (it == peer_addresses.end()) {
            continue;
        }
        
        const std::string& address = it->second;
        
        // 在新线程中发送 RPC，避免阻塞主循环
        std::thread([raft_node, peer_id, address, i]() {
            auto client = network::createRPCClient(address);
            if (client->connect(address)) {
                // 创建 AppendEntries 参数（nodeIndex 是在 peerIds_ 数组中的索引）
                raft::AppendEntriesArgs args = raft_node->createAppendEntriesArgs(i);
                raft::AppendEntriesReply reply;
                
                if (client->sendAppendEntries(args, reply, 1000)) {
                    // 处理 AppendEntries 响应
                    raft_node->handleAppendEntriesResponse(peer_id, reply);
                }
                client->disconnect();
            }
        }).detach();
    }
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <node_id> [config_file]" << std::endl;
    std::cout << "  node_id: Node ID (1, 2, 3, ...)" << std::endl;
    std::cout << "  config_file: Path to cluster configuration file (default: config/cluster.conf)" << std::endl;
    std::cout << std::endl;
    std::cout << "Example:" << std::endl;
    std::cout << "  " << program_name << " 1" << std::endl;
    std::cout << "  " << program_name << " 2 config/cluster.conf" << std::endl;
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    int node_id = std::atoi(argv[1]);
    std::string config_file = (argc >= 3) ? argv[2] : "config/cluster.conf";
    
    std::cout << "========================================" << std::endl;
    std::cout << "  Raft KV Store Node Server" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Node ID: " << node_id << std::endl;
    std::cout << "Config File: " << config_file << std::endl;
    std::cout << std::endl;
    
    // 加载配置
    util::ClusterConfig config;
    if (!config.loadFromFile(config_file)) {
        std::cerr << "Failed to load configuration from " << config_file << std::endl;
        return 1;
    }
    
    // 获取当前节点配置
    util::NodeConfig node_config = config.getNodeConfig(node_id);
    if (node_config.id == 0) {
        std::cerr << "Node " << node_id << " not found in configuration" << std::endl;
        return 1;
    }
    
    std::cout << "Node Configuration:" << std::endl;
    std::cout << "  Address: " << node_config.getAddress() << std::endl;
    std::cout << "  Data Directory: " << node_config.data_dir << std::endl;
    std::cout << std::endl;
    
    // 获取对等节点列表
    std::vector<int> peer_ids;
    std::map<int, std::string> peer_addresses;
    auto all_nodes = config.getAllNodes();
    
    for (const auto& node : all_nodes) {
        if (node.id != node_id) {
            peer_ids.push_back(node.id);
            peer_addresses[node.id] = node.getAddress();
        }
    }
    
    std::cout << "Cluster Configuration:" << std::endl;
    std::cout << "  Total Nodes: " << all_nodes.size() << std::endl;
    std::cout << "  Peer Nodes: ";
    for (size_t i = 0; i < peer_ids.size(); ++i) {
        std::cout << peer_ids[i];
        if (i < peer_ids.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl << std::endl;
    
    try {
        // 创建 KV 存储
        std::cout << "Initializing KV Store..." << std::endl;
        auto kv_store = std::make_shared<raft_kv::KVStore>();
        
        // 创建 Raft 节点
        std::cout << "Initializing Raft Node..." << std::endl;
        auto raft_node = std::make_shared<raft::RaftNode>(node_id, peer_ids);
        raft_node->setNodeAddress(node_config.getAddress());
        raft_node->setPeerAddresses(peer_addresses);
        
        // 设置状态机应用回调
        raft_node->setApplyCallback([kv_store](const std::string& command) -> std::string {
            return kv_store->applyCommandWithResult(command);
        });
        
        // 创建持久化器
        std::cout << "Initializing Persister..." << std::endl;
        auto persister = std::make_shared<storage::Persister>(node_config.data_dir);
        
        // 尝试从持久化存储恢复状态
        int current_term = 0;
        int voted_for = -1;
        std::vector<raft::LogEntry> log;
        
        if (persister->loadRaftState(current_term, voted_for, log)) {
            std::cout << "Recovered state from persistent storage:" << std::endl;
            std::cout << "  Term: " << current_term << std::endl;
            std::cout << "  Voted For: " << voted_for << std::endl;
            std::cout << "  Log Entries: " << log.size() << std::endl;
            // TODO: 将恢复的状态应用到 RaftNode
        } else {
            std::cout << "No previous state found, starting fresh" << std::endl;
        }
        std::cout << std::endl;
        
        // 创建并启动 RPC 服务器
        std::cout << "Starting RPC Server on " << node_config.getAddress() << "..." << std::endl;
        g_server = network::createRPCServer(raft_node.get());
        
        if (!g_server->start(node_config.getAddress())) {
            std::cerr << "Failed to start RPC server" << std::endl;
            return 1;
        }
        
        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "  Node " << node_id << " is running!" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;
        
        // 注册信号处理器
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // 主循环：定期检查选举超时和发送心跳
        auto last_status_print = std::chrono::steady_clock::now();
        const auto status_interval = std::chrono::seconds(5);
        
        while (g_running) {
            // 检查选举超时
            if (raft_node->checkElectionTimeout()) {
                raft_node->startElection();
                // 向所有对等节点发送投票请求
                sendRequestVotesToPeers(raft_node.get(), node_id, peer_addresses);
            }
            
            // Leader 发送心跳
            if (raft_node->isLeader() && raft_node->shouldSendHeartbeat()) {
                // 向所有对等节点发送 AppendEntries RPC（心跳）
                sendAppendEntriesToPeers(raft_node.get(), peer_ids, peer_addresses);
                raft_node->resetHeartbeatTimer();
            }
            
            // 应用已提交的日志
            raft_node->applyCommittedEntries();
            
            // 定期打印状态
            auto now = std::chrono::steady_clock::now();
            if (now - last_status_print >= status_interval) {
                std::string state_str;
                switch (raft_node->getState()) {
                    case raft::NodeState::FOLLOWER: state_str = "FOLLOWER"; break;
                    case raft::NodeState::CANDIDATE: state_str = "CANDIDATE"; break;
                    case raft::NodeState::LEADER: state_str = "LEADER"; break;
                }
                
                std::cout << "[Status] Term: " << raft_node->getCurrentTerm()
                         << ", State: " << state_str
                         << ", Leader: " << raft_node->getLeaderId()
                         << ", Log Size: " << raft_node->getLogSize()
                         << ", Commit Index: " << raft_node->getCommitIndex()
                         << std::endl;
                
                last_status_print = now;
            }
            
            // 短暂休眠
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // 优雅关闭
        std::cout << std::endl;
        std::cout << "Shutting down..." << std::endl;
        
        // 保存状态
        std::cout << "Saving state to persistent storage..." << std::endl;
        // TODO: 从 RaftNode 获取当前状态并保存
        
        // 停止 RPC 服务器
        std::cout << "Stopping RPC server..." << std::endl;
        g_server->stop();
        
        std::cout << "Node " << node_id << " stopped successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
