#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "client/kv_client.h"
#include "util/config.h"

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [command] [args...] [config_file]" << std::endl;
    std::cout << std::endl;
    std::cout << "Interactive mode:" << std::endl;
    std::cout << "  " << program_name << " [config_file]" << std::endl;
    std::cout << std::endl;
    std::cout << "Non-interactive mode:" << std::endl;
    std::cout << "  " << program_name << " PUT <key> <value> [config_file]" << std::endl;
    std::cout << "  " << program_name << " GET <key> [config_file]" << std::endl;
    std::cout << "  " << program_name << " DELETE <key> [config_file]" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  put <key> <value>  - Store a key-value pair" << std::endl;
    std::cout << "  get <key>          - Retrieve value for a key" << std::endl;
    std::cout << "  delete <key>       - Delete a key-value pair" << std::endl;
    std::cout << "  help               - Show this help message" << std::endl;
    std::cout << "  quit               - Exit the client" << std::endl;
}

int main(int argc, char* argv[]) {
    // 检查是否为非交互模式（命令行参数）
    bool interactive_mode = true;
    std::string config_file = "config/cluster.conf";
    std::string command;
    std::vector<std::string> args;
    
    if (argc >= 2) {
        std::string first_arg = argv[1];
        // 转换为大写以便比较
        std::string upper_arg = first_arg;
        for (auto& c : upper_arg) c = std::toupper(c);
        
        if (upper_arg == "PUT" || upper_arg == "GET" || upper_arg == "DELETE") {
            // 非交互模式
            interactive_mode = false;
            command = upper_arg;
            
            // 收集参数
            for (int i = 2; i < argc; ++i) {
                args.push_back(argv[i]);
            }
            
            // 最后一个参数可能是配置文件（如果它看起来像文件路径）
            if (!args.empty()) {
                std::string last_arg = args.back();
                if (last_arg.find(".conf") != std::string::npos || 
                    last_arg.find("/") != std::string::npos) {
                    config_file = last_arg;
                    args.pop_back();
                }
            }
        } else {
            // 交互模式，第一个参数是配置文件
            config_file = argv[1];
        }
    }
    
    // 非交互模式：静默输出
    if (!interactive_mode) {
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
        
        // 创建客户端
        client::KVClient kv_client(server_addresses);
        kv_client.setTimeout(5000);  // 5 秒超时
        kv_client.setMaxRetries(3);
        
        if (!kv_client.connect()) {
            std::cerr << "Failed to connect to cluster" << std::endl;
            return 1;
        }
        
        // 执行命令
        bool success = false;
        
        if (command == "PUT") {
            if (args.size() < 2) {
                std::cerr << "Usage: " << argv[0] << " PUT <key> <value>" << std::endl;
                return 1;
            }
            std::string key = args[0];
            std::string value = args[1];
            
            // 如果有更多参数，将它们连接到 value
            for (size_t i = 2; i < args.size(); ++i) {
                value += " " + args[i];
            }
            
            success = kv_client.put(key, value);
            if (success) {
                std::cout << "OK" << std::endl;
            } else {
                std::cerr << "Error: " << kv_client.getLastError() << std::endl;
            }
        }
        else if (command == "GET") {
            if (args.empty()) {
                std::cerr << "Usage: " << argv[0] << " GET <key>" << std::endl;
                return 1;
            }
            std::string key = args[0];
            std::string value;
            
            success = kv_client.get(key, value);
            if (success) {
                std::cout << value << std::endl;
            } else {
                std::cerr << "Error: " << kv_client.getLastError() << std::endl;
            }
        }
        else if (command == "DELETE") {
            if (args.empty()) {
                std::cerr << "Usage: " << argv[0] << " DELETE <key>" << std::endl;
                return 1;
            }
            std::string key = args[0];
            
            success = kv_client.remove(key);
            if (success) {
                std::cout << "OK" << std::endl;
            } else {
                std::cerr << "Error: " << kv_client.getLastError() << std::endl;
            }
        }
        
        kv_client.disconnect();
        return success ? 0 : 1;
    }
    
    // 交互模式
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
