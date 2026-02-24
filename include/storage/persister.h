#ifndef STORAGE_PERSISTER_H
#define STORAGE_PERSISTER_H

#include <string>
#include <vector>
#include <mutex>
#include "raft/log_entry.h"

namespace storage {

/**
 * @brief Raft 状态持久化类
 * 
 * 负责将 Raft 节点的关键状态持久化到磁盘，确保节点崩溃后能够恢复。
 * 使用原子性写入（先写临时文件，再重命名）保证数据完整性。
 * 
 * 需求: 2.1.3 (安全性保证 - 持久化存储)
 * 
 * 持久化的状态包括：
 * 1. currentTerm: 当前任期号
 * 2. votedFor: 当前任期投票给了哪个节点（-1 表示未投票）
 * 3. log[]: 日志条目数组
 */
class Persister {
public:
    /**
     * @brief 构造函数
     * @param dataDir 数据目录路径，用于存储持久化文件
     */
    explicit Persister(const std::string& dataDir);

    /**
     * @brief 析构函数
     */
    ~Persister() = default;

    /**
     * @brief 保存 Raft 状态到磁盘
     * 
     * 使用原子性写入：先写入临时文件，写入成功后再重命名为正式文件。
     * 这样可以避免写入过程中崩溃导致的数据损坏。
     * 
     * @param currentTerm 当前任期号
     * @param votedFor 投票给的节点 ID（-1 表示未投票）
     * @param log 日志条目数组
     * @return true 保存成功，false 保存失败
     */
    bool saveRaftState(int currentTerm, int votedFor, 
                       const std::vector<raft::LogEntry>& log);

    /**
     * @brief 从磁盘加载 Raft 状态
     * 
     * 如果文件不存在或读取失败，返回 false，参数保持不变。
     * 如果文件存在但格式错误，返回 false。
     * 
     * @param currentTerm 输出参数：当前任期号
     * @param votedFor 输出参数：投票给的节点 ID
     * @param log 输出参数：日志条目数组
     * @return true 加载成功，false 加载失败或文件不存在
     */
    bool loadRaftState(int& currentTerm, int& votedFor, 
                       std::vector<raft::LogEntry>& log);

    /**
     * @brief 检查持久化文件是否存在
     * @return true 文件存在，false 文件不存在
     */
    bool exists() const;

    /**
     * @brief 清除持久化数据（用于测试）
     * @return true 清除成功，false 清除失败
     */
    bool clear();

private:
    std::string dataDir_;           // 数据目录
    std::string stateFile_;         // 状态文件路径
    std::string tempFile_;          // 临时文件路径
    mutable std::mutex mutex_;      // 保护并发访问

    /**
     * @brief 确保数据目录存在
     * @return true 目录存在或创建成功，false 创建失败
     */
    bool ensureDataDir();

    /**
     * @brief 序列化 Raft 状态为字符串
     * 
     * 格式：
     * Line 1: currentTerm
     * Line 2: votedFor
     * Line 3: log_count
     * Line 4+: 每个日志条目使用长度前缀格式：
     *   - 一行：日志条目序列化字符串的长度
     *   - 紧接着：日志条目的序列化字符串（不含换行符）
     * 
     * @param currentTerm 当前任期号
     * @param votedFor 投票给的节点 ID
     * @param log 日志条目数组
     * @return 序列化后的字符串
     */
    std::string serializeState(int currentTerm, int votedFor,
                               const std::vector<raft::LogEntry>& log) const;

    /**
     * @brief 从字符串反序列化 Raft 状态
     * 
     * @param data 序列化的字符串
     * @param currentTerm 输出参数：当前任期号
     * @param votedFor 输出参数：投票给的节点 ID
     * @param log 输出参数：日志条目数组
     * @return true 反序列化成功，false 格式错误
     */
    bool deserializeState(const std::string& data, int& currentTerm, 
                         int& votedFor, std::vector<raft::LogEntry>& log) const;
};

} // namespace storage

#endif // STORAGE_PERSISTER_H
