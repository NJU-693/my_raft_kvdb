#include "util/config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace util {

bool ClusterConfig::loadFromFile(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_file << std::endl;
        return false;
    }
    
    std::string line;
    std::string current_section;
    NodeConfig current_node;
    int current_node_id = -1;
    
    while (std::getline(file, line)) {
        // 移除注释和空白
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // 去除首尾空白
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty()) {
            continue;
        }
        
        // 检查是否是节（section）
        if (line.front() == '[' && line.back() == ']') {
            // 保存上一个节点配置
            if (current_node_id > 0) {
                nodes_[current_node_id] = current_node;
                current_node = NodeConfig();
            }
            
            current_section = line.substr(1, line.length() - 2);
            
            // 检查是否是节点配置节
            if (current_section.find("node_") == 0) {
                current_node_id = std::stoi(current_section.substr(5));
                current_node.id = current_node_id;
            } else {
                current_node_id = -1;
            }
            continue;
        }
        
        // 解析键值对
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // 去除键值的空白
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // 根据当前节处理键值对
        if (current_section == "cluster") {
            if (key == "node_count") {
                node_count_ = std::stoi(value);
            }
        } else if (current_section == "raft") {
            if (key == "election_timeout_min") {
                election_timeout_min_ = std::stoi(value);
            } else if (key == "election_timeout_max") {
                election_timeout_max_ = std::stoi(value);
            } else if (key == "heartbeat_interval") {
                heartbeat_interval_ = std::stoi(value);
            } else if (key == "rpc_timeout") {
                rpc_timeout_ = std::stoi(value);
            }
        } else if (current_node_id > 0) {
            if (key == "host") {
                current_node.host = value;
            } else if (key == "port") {
                current_node.port = std::stoi(value);
            } else if (key == "data_dir") {
                current_node.data_dir = value;
            }
        }
    }
    
    // 保存最后一个节点配置
    if (current_node_id > 0) {
        nodes_[current_node_id] = current_node;
    }
    
    file.close();
    
    // 验证配置
    if (nodes_.empty()) {
        std::cerr << "No nodes configured" << std::endl;
        return false;
    }
    
    return true;
}

NodeConfig ClusterConfig::getNodeConfig(int node_id) const {
    auto it = nodes_.find(node_id);
    if (it != nodes_.end()) {
        return it->second;
    }
    return NodeConfig();
}

std::vector<NodeConfig> ClusterConfig::getAllNodes() const {
    std::vector<NodeConfig> result;
    for (const auto& pair : nodes_) {
        result.push_back(pair.second);
    }
    return result;
}

} // namespace util
