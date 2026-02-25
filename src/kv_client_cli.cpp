#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "client/kv_client.h"
#include "util/config.h"

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [config_file]" << std::endl;
    std::cout << "  config_file: Path to cluster configuration file (default: config/cluster.conf)" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  put <key> <value>  - Store a key-value pair" << std::endl;
    std::cout << "  get <key>          - Retrieve value for a key" << std::endl;
    std::cout << "  delete <key>       - Delete a key-value pair" << std::endl;
    std::cout << "  help               - Show this help message" << std::endl;
    std::cout << "  quit               - Exit the client" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file = (argc >= 2) ? argv[1] : "config/cluster.conf";
    
    std::cout << "========================================" << std::endl;
    std::cout << "  Raft KV Store Client" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Config File: " << config_file << std::endl;
    std::cout << std::endl;
    
    // 加载配置
    util::ClusterConfig config;
    if (!config.loadFromFile(config_file)) {
        std::cerr << "Failed to load configuration from " << config_file << std::endl;
        return 1;
    }
    
    // 获取所有节点地址
    std::vector<std::string> server_addresses;
    auto all_nodes = config.getAllNodes();
    
    for (const auto& node : all_nodes) {
        server_addresses.push_back(node.getAddress());
    }
    
    std::cout << "Cluster nodes:" << std::endl;
    for (const auto& addr : server_addresses) {
        std::cout << "  - " << addr << std::endl;
    }
    std::cout << std::endl;
    
    // 创建客户端
    client::KVClient kv_client(server_addresses);
    kv_client.setTimeout(5000);  // 5 秒超时
    kv_client.setMaxRetries(3);
    
    std::cout << "Connecting to cluster..." << std::endl;
    if (!kv_client.connect()) {
        std::cerr << "Failed to connect to cluster" << std::endl;
        return 1;
    }
    
    std::cout << "Connected successfully!" << std::endl;
    std::cout << std::endl;
    std::cout << "Type 'help' for available commands" << std::endl;
    std::cout << std::endl;
    
    // 命令循环
    std::string line;
    while (true) {
        std::cout << "raft-kv> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        // 解析命令
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        if (command.empty()) {
            continue;
        }
        
        if (command == "quit" || command == "exit") {
            break;
        }
        
        if (command == "help") {
            printUsage(argv[0]);
            continue;
        }
        
        if (command == "put") {
            std::string key, value;
            iss >> key;
            
            // 读取剩余部分作为 value（可能包含空格）
            std::getline(iss, value);
            // 去除前导空格
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos) {
                value = value.substr(start);
            }
            
            if (key.empty() || value.empty()) {
                std::cerr << "Usage: put <key> <value>" << std::endl;
                continue;
            }
            
            if (kv_client.put(key, value)) {
                std::cout << "OK" << std::endl;
            } else {
                std::cerr << "Error: " << kv_client.getLastError() << std::endl;
            }
        }
        else if (command == "get") {
            std::string key;
            iss >> key;
            
            if (key.empty()) {
                std::cerr << "Usage: get <key>" << std::endl;
                continue;
            }
            
            std::string value;
            if (kv_client.get(key, value)) {
                std::cout << value << std::endl;
            } else {
                std::cerr << "Error: " << kv_client.getLastError() << std::endl;
            }
        }
        else if (command == "delete" || command == "del") {
            std::string key;
            iss >> key;
            
            if (key.empty()) {
                std::cerr << "Usage: delete <key>" << std::endl;
                continue;
            }
            
            if (kv_client.remove(key)) {
                std::cout << "OK" << std::endl;
            } else {
                std::cerr << "Error: " << kv_client.getLastError() << std::endl;
            }
        }
        else {
            std::cerr << "Unknown command: " << command << std::endl;
            std::cerr << "Type 'help' for available commands" << std::endl;
        }
    }
    
    std::cout << std::endl;
    std::cout << "Disconnecting..." << std::endl;
    kv_client.disconnect();
    std::cout << "Goodbye!" << std::endl;
    
    return 0;
}
