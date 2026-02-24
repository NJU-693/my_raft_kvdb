#ifndef CLIENT_KV_CLIENT_H
#define CLIENT_KV_CLIENT_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace client {

/**
 * @brief KV 客户端类
 * 
 * 提供连接到 Raft 集群并执行 KV 操作的客户端接口。
 * 支持自动 Leader 重定向和故障重试。
 * 
 * 需求: 2.3.2 (客户端-服务器通信)
 */
class KVClient {
public:
    /**
     * @brief 构造函数
     * @param server_addresses 集群节点地址列表（格式: "host:port"）
     */
    explicit KVClient(const std::vector<std::string>& server_addresses);

    /**
     * @brief 析构函数
     */
    ~KVClient();

    /**
     * @brief 连接到集群
     * @return 是否成功连接
     */
    bool connect();

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief Put 操作：存储或更新键值对
     * @param key 键
     * @param value 值
     * @return 是否成功
     */
    bool put(const std::string& key, const std::string& value);

    /**
     * @brief Get 操作：获取指定键的值
     * @param key 键
     * @param value 值（输出参数）
     * @return 是否成功（如果 key 不存在返回 false）
     */
    bool get(const std::string& key, std::string& value);

    /**
     * @brief Delete 操作：删除指定的键值对
     * @param key 键
     * @return 是否成功
     */
    bool remove(const std::string& key);

    /**
     * @brief 设置超时时间
     * @param timeout_ms 超时时间（毫秒）
     */
    void setTimeout(int timeout_ms);

    /**
     * @brief 设置最大重试次数
     * @param max_retries 最大重试次数
     */
    void setMaxRetries(int max_retries);

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息
     */
    std::string getLastError() const;

private:
    std::vector<std::string> server_addresses_;  // 集群节点地址列表
    std::string current_leader_;                 // 当前已知的 Leader 地址
    int timeout_ms_;                             // 超时时间（毫秒）
    int max_retries_;                            // 最大重试次数
    std::string last_error_;                     // 最后一次错误信息
    mutable std::mutex mutex_;                   // 保护并发访问

    /**
     * @brief 发送请求到指定服务器
     * @param server_address 服务器地址
     * @param operation 操作类型（"PUT", "GET", "DELETE"）
     * @param key 键
     * @param value 值（PUT 操作使用）
     * @param result 结果（GET 操作使用）
     * @return 是否成功
     */
    bool sendRequest(
        const std::string& server_address,
        const std::string& operation,
        const std::string& key,
        const std::string& value,
        std::string& result);

    /**
     * @brief 执行操作（带自动重定向和重试）
     * @param operation 操作类型
     * @param key 键
     * @param value 值
     * @param result 结果
     * @return 是否成功
     */
    bool executeOperation(
        const std::string& operation,
        const std::string& key,
        const std::string& value,
        std::string& result);

    /**
     * @brief 查找 Leader
     * @return Leader 地址（如果找到），否则返回空字符串
     */
    std::string findLeader();
};

} // namespace client

#endif // CLIENT_KV_CLIENT_H
