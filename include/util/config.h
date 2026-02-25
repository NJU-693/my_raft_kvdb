#ifndef UTIL_CONFIG_H
#define UTIL_CONFIG_H

#include <string>
#include <map>
#include <vector>

namespace util {

/**
 * @brief 节点配置结构
 */
struct NodeConfig {
    int id;
    std::string host;
    int port;
    std::string data_dir;
    
    std::string getAddress() const {
        return host + ":" + std::to_string(port);
    }
};

/**
 * @brief 集群配置类
 * 
 * 从配置文件读取集群配置信息
 */
class ClusterConfig {
public:
    /**
     * @brief 从文件加载配置
     * @param config_file 配置文件路径
     * @return 是否成功加载
     */
    bool loadFromFile(const std::string& config_file);
    
    /**
     * @brief 获取节点配置
     * @param node_id 节点 ID
     * @return 节点配置（如果不存在返回空）
     */
    NodeConfig getNodeConfig(int node_id) const;
    
    /**
     * @brief 获取所有节点配置
     */
    std::vector<NodeConfig> getAllNodes() const;
    
    /**
     * @brief 获取节点数量
     */
    int getNodeCount() const { return node_count_; }
    
    /**
     * @brief 获取选举超时最小值（毫秒）
     */
    int getElectionTimeoutMin() const { return election_timeout_min_; }
    
    /**
     * @brief 获取选举超时最大值（毫秒）
     */
    int getElectionTimeoutMax() const { return election_timeout_max_; }
    
    /**
     * @brief 获取心跳间隔（毫秒）
     */
    int getHeartbeatInterval() const { return heartbeat_interval_; }
    
    /**
     * @brief 获取 RPC 超时时间（毫秒）
     */
    int getRpcTimeout() const { return rpc_timeout_; }

private:
    int node_count_ = 0;
    std::map<int, NodeConfig> nodes_;
    
    // Raft 参数
    int election_timeout_min_ = 150;
    int election_timeout_max_ = 300;
    int heartbeat_interval_ = 50;
    int rpc_timeout_ = 1000;
    
    /**
     * @brief 解析配置行
     */
    bool parseLine(const std::string& line, std::string& section, 
                   std::string& key, std::string& value);
};

} // namespace util

#endif // UTIL_CONFIG_H
