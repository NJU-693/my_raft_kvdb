#ifndef COMMAND_H
#define COMMAND_H

#include <string>

namespace raft_kv {

/**
 * @brief 命令类型枚举
 * 
 * 定义 KV 存储支持的三种基本操作类型
 */
enum class CommandType {
    PUT,     // 插入或更新键值对
    GET,     // 查询键对应的值
    DELETE   // 删除键值对
};

/**
 * @brief 命令类
 * 
 * 表示一个 KV 操作命令，包含操作类型、键和值（可选）
 * 支持序列化和反序列化，用于在 Raft 日志中存储和传输
 */
class Command {
public:
    /**
     * @brief 默认构造函数
     */
    Command();

    /**
     * @brief 构造函数
     * @param type 命令类型
     * @param key 键
     * @param value 值（仅 PUT 命令使用）
     */
    Command(CommandType type, const std::string& key, const std::string& value = "");

    /**
     * @brief 获取命令类型
     * @return 命令类型
     */
    CommandType getType() const;

    /**
     * @brief 获取键
     * @return 键
     */
    const std::string& getKey() const;

    /**
     * @brief 获取值
     * @return 值
     */
    const std::string& getValue() const;

    /**
     * @brief 序列化命令为字符串
     * 
     * 格式：
     * - PUT 命令: "PUT:key:value"
     * - GET 命令: "GET:key"
     * - DELETE 命令: "DELETE:key"
     * 
     * @return 序列化后的字符串
     */
    std::string serialize() const;

    /**
     * @brief 从字符串反序列化命令
     * 
     * @param str 序列化的命令字符串
     * @return 反序列化后的 Command 对象
     * @throws std::invalid_argument 如果字符串格式无效
     */
    static Command deserialize(const std::string& str);

private:
    CommandType type_;   // 命令类型
    std::string key_;    // 键
    std::string value_;  // 值（仅 PUT 使用）
};

}  // namespace raft_kv

#endif  // COMMAND_H
