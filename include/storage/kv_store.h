#ifndef RAFT_KV_STORE_KV_STORE_H
#define RAFT_KV_STORE_KV_STORE_H

#include <mutex>
#include <string>
#include "skiplist.h"

namespace raft_kv {

/**
 * @brief KV 存储引擎类
 * 
 * 基于跳表实现的键值存储引擎，提供线程安全的 Put/Get/Delete 操作。
 * 该类是 Raft KV 系统的存储层，负责实际的数据存储和检索。
 * 
 * 特性：
 * - 基于 SkipList<string, string> 实现高效存储
 * - 使用 mutex 保护并发访问，确保线程安全
 * - 支持命令应用接口，用于 Raft 日志回放
 * 
 * 线程安全：所有公共方法都是线程安全的
 */
class KVStore {
public:
    /**
     * @brief 构造函数
     * 
     * 初始化 KV 存储引擎，创建底层跳表数据结构
     */
    KVStore();

    /**
     * @brief 析构函数
     * 
     * 清理资源，跳表会自动释放所有节点
     */
    ~KVStore();

    // 禁用拷贝构造和赋值操作（KVStore 不应被拷贝）
    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;

    /**
     * @brief 插入或更新键值对
     * 
     * 如果 key 不存在，创建新条目；如果 key 已存在，更新 value。
     * 
     * @param key 键
     * @param value 值
     * @return true 表示插入新键，false 表示更新已有键
     * 
     * 线程安全：是
     */
    bool put(const std::string& key, const std::string& value);

    /**
     * @brief 获取指定键的值
     * 
     * @param key 键
     * @param value 输出参数，存储找到的值
     * @return true 表示找到，false 表示 key 不存在
     * 
     * 线程安全：是
     */
    bool get(const std::string& key, std::string& value);

    /**
     * @brief 删除指定的键值对
     * 
     * @param key 要删除的键
     * @return true 表示成功删除，false 表示 key 不存在
     * 
     * 线程安全：是
     */
    bool remove(const std::string& key);

    /**
     * @brief 应用命令到存储引擎
     * 
     * 解析命令字符串并执行相应的 KV 操作。
     * 该方法用于 Raft 日志回放，将已提交的命令应用到状态机。
     * 
     * 命令格式：
     * - PUT 命令: "PUT:key:value"
     * - GET 命令: "GET:key"
     * - DELETE 命令: "DELETE:key"
     * 
     * @param command 序列化的命令字符串
     * 
     * 线程安全：是
     * 
     * @throws std::invalid_argument 如果命令格式无效
     */
    void applyCommand(const std::string& command);

    /**
     * @brief 获取存储的键值对数量
     * 
     * @return 当前存储的元素数量
     * 
     * 线程安全：是
     */
    int size() const;

private:
    SkipList<std::string, std::string> store_;  // 底层跳表存储
    mutable std::mutex mtx_;                     // 保护并发访问的互斥锁
};

}  // namespace raft_kv

#endif  // RAFT_KV_STORE_KV_STORE_H
