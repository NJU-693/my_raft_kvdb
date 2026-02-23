#ifndef RAFT_LOG_ENTRY_H
#define RAFT_LOG_ENTRY_H

#include <string>
#include <sstream>
#include <stdexcept>

namespace raft {

/**
 * @brief 日志条目结构
 * 
 * 表示 Raft 日志中的一个条目，包含任期号、命令和索引。
 * 每个日志条目对应一个状态机命令（如 KV 操作）。
 * 
 * 需求: 2.1.2 (日志复制)
 */
class LogEntry {
public:
    int term;           // 日志条目的任期号
    std::string command; // 状态机命令（序列化的 KV 操作，如 "PUT:key:value"）
    int index;          // 日志索引（从 1 开始）

    /**
     * @brief 默认构造函数
     */
    LogEntry() : term(0), command(""), index(0) {}

    /**
     * @brief 构造函数
     * @param t 任期号
     * @param cmd 命令字符串
     * @param idx 日志索引
     */
    LogEntry(int t, const std::string& cmd, int idx)
        : term(t), command(cmd), index(idx) {}

    /**
     * @brief 序列化为字符串
     * 格式: "term|index|length|command"
     * 使用长度前缀避免命令中的分隔符问题
     * @return 序列化后的字符串
     */
    std::string serialize() const {
        std::ostringstream oss;
        oss << term << "|" << index << "|" << command.length() << "|" << command;
        return oss.str();
    }

    /**
     * @brief 从字符串反序列化
     * @param str 序列化的字符串
     * @return LogEntry 对象
     * @throws std::invalid_argument 如果格式不正确
     */
    static LogEntry deserialize(const std::string& str) {
        std::istringstream iss(str);
        std::string term_str, index_str, length_str;
        
        if (!std::getline(iss, term_str, '|') ||
            !std::getline(iss, index_str, '|') ||
            !std::getline(iss, length_str, '|')) {
            throw std::invalid_argument("Invalid LogEntry format");
        }

        try {
            int term = std::stoi(term_str);
            int index = std::stoi(index_str);
            size_t length = std::stoull(length_str);
            
            // 读取指定长度的命令
            std::string command(length, '\0');
            iss.read(&command[0], length);
            
            if (iss.gcount() != static_cast<std::streamsize>(length)) {
                throw std::invalid_argument("Command length mismatch");
            }
            
            return LogEntry(term, command, index);
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid LogEntry format: " + std::string(e.what()));
        }
    }

    /**
     * @brief 比较运算符
     */
    bool operator==(const LogEntry& other) const {
        return term == other.term && 
               command == other.command && 
               index == other.index;
    }

    bool operator!=(const LogEntry& other) const {
        return !(*this == other);
    }
};

} // namespace raft

#endif // RAFT_LOG_ENTRY_H
