#ifndef RAFT_RPC_MESSAGES_H
#define RAFT_RPC_MESSAGES_H

#include "log_entry.h"
#include <vector>
#include <sstream>
#include <stdexcept>

namespace raft {

/**
 * @brief RequestVote RPC 请求参数
 * 
 * 候选人用于请求其他节点投票的消息。
 * 需求: 2.1.1 (领导者选举), 2.3.1 (节点间 RPC 通信)
 */
struct RequestVoteArgs {
    int term;           // 候选人的任期号
    int candidateId;    // 候选人的 ID
    int lastLogIndex;   // 候选人最后日志条目的索引
    int lastLogTerm;    // 候选人最后日志条目的任期号

    RequestVoteArgs()
        : term(0), candidateId(0), lastLogIndex(0), lastLogTerm(0) {}

    RequestVoteArgs(int t, int cid, int lli, int llt)
        : term(t), candidateId(cid), lastLogIndex(lli), lastLogTerm(llt) {}

    /**
     * @brief 序列化为字符串
     * 格式: "term|candidateId|lastLogIndex|lastLogTerm"
     */
    std::string serialize() const {
        std::ostringstream oss;
        oss << term << "|" << candidateId << "|" 
            << lastLogIndex << "|" << lastLogTerm;
        return oss.str();
    }

    /**
     * @brief 从字符串反序列化
     */
    static RequestVoteArgs deserialize(const std::string& str) {
        std::istringstream iss(str);
        std::string term_str, cid_str, lli_str, llt_str;
        
        if (!std::getline(iss, term_str, '|') ||
            !std::getline(iss, cid_str, '|') ||
            !std::getline(iss, lli_str, '|') ||
            !std::getline(iss, llt_str)) {
            throw std::invalid_argument("Invalid RequestVoteArgs format");
        }

        try {
            return RequestVoteArgs(
                std::stoi(term_str),
                std::stoi(cid_str),
                std::stoi(lli_str),
                std::stoi(llt_str)
            );
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid RequestVoteArgs format: " + 
                                      std::string(e.what()));
        }
    }
};

/**
 * @brief RequestVote RPC 响应
 * 
 * 节点对投票请求的响应。
 * 需求: 2.1.1 (领导者选举), 2.3.1 (节点间 RPC 通信)
 */
struct RequestVoteReply {
    int term;           // 当前任期号，用于候选人更新自己
    bool voteGranted;   // 是否投票给候选人

    RequestVoteReply() : term(0), voteGranted(false) {}

    RequestVoteReply(int t, bool granted)
        : term(t), voteGranted(granted) {}

    /**
     * @brief 序列化为字符串
     * 格式: "term|voteGranted"
     */
    std::string serialize() const {
        std::ostringstream oss;
        oss << term << "|" << (voteGranted ? "1" : "0");
        return oss.str();
    }

    /**
     * @brief 从字符串反序列化
     */
    static RequestVoteReply deserialize(const std::string& str) {
        std::istringstream iss(str);
        std::string term_str, granted_str;
        
        if (!std::getline(iss, term_str, '|') ||
            !std::getline(iss, granted_str)) {
            throw std::invalid_argument("Invalid RequestVoteReply format");
        }

        try {
            return RequestVoteReply(
                std::stoi(term_str),
                granted_str == "1"
            );
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid RequestVoteReply format: " + 
                                      std::string(e.what()));
        }
    }
};

/**
 * @brief AppendEntries RPC 请求参数
 * 
 * Leader 用于复制日志条目和发送心跳的消息。
 * 需求: 2.1.2 (日志复制), 2.3.1 (节点间 RPC 通信)
 */
struct AppendEntriesArgs {
    int term;                       // Leader 的任期号
    int leaderId;                   // Leader 的 ID，便于 Follower 重定向客户端
    int prevLogIndex;               // 新日志条目之前的日志索引
    int prevLogTerm;                // prevLogIndex 处日志条目的任期号
    std::vector<LogEntry> entries;  // 要存储的日志条目（心跳时为空）
    int leaderCommit;               // Leader 的 commitIndex

    AppendEntriesArgs()
        : term(0), leaderId(0), prevLogIndex(0), 
          prevLogTerm(0), leaderCommit(0) {}

    AppendEntriesArgs(int t, int lid, int pli, int plt, 
                     const std::vector<LogEntry>& ents, int lc)
        : term(t), leaderId(lid), prevLogIndex(pli), 
          prevLogTerm(plt), entries(ents), leaderCommit(lc) {}

    /**
     * @brief 序列化为字符串
     * 格式: "term|leaderId|prevLogIndex|prevLogTerm|leaderCommit|entryCount|entry1_serialized|entry2_serialized|..."
     */
    std::string serialize() const {
        std::ostringstream oss;
        oss << term << "|" << leaderId << "|" 
            << prevLogIndex << "|" << prevLogTerm << "|" 
            << leaderCommit << "|" << entries.size();
        
        for (const auto& entry : entries) {
            std::string entry_str = entry.serialize();
            oss << "|" << entry_str.length() << "|" << entry_str;
        }
        
        return oss.str();
    }

    /**
     * @brief 从字符串反序列化
     */
    static AppendEntriesArgs deserialize(const std::string& str) {
        std::istringstream iss(str);
        std::string term_str, lid_str, pli_str, plt_str, lc_str, count_str;
        
        if (!std::getline(iss, term_str, '|') ||
            !std::getline(iss, lid_str, '|') ||
            !std::getline(iss, pli_str, '|') ||
            !std::getline(iss, plt_str, '|') ||
            !std::getline(iss, lc_str, '|') ||
            !std::getline(iss, count_str, '|')) {
            throw std::invalid_argument("Invalid AppendEntriesArgs format");
        }

        try {
            AppendEntriesArgs args;
            args.term = std::stoi(term_str);
            args.leaderId = std::stoi(lid_str);
            args.prevLogIndex = std::stoi(pli_str);
            args.prevLogTerm = std::stoi(plt_str);
            args.leaderCommit = std::stoi(lc_str);
            
            int count = std::stoi(count_str);
            args.entries.reserve(count);
            
            for (int i = 0; i < count; ++i) {
                std::string length_str;
                if (!std::getline(iss, length_str, '|')) {
                    throw std::invalid_argument("Missing entry length");
                }
                
                size_t length = std::stoull(length_str);
                std::string entry_str(length, '\0');
                iss.read(&entry_str[0], length);
                
                if (iss.gcount() != static_cast<std::streamsize>(length)) {
                    throw std::invalid_argument("Entry length mismatch");
                }
                
                // 跳过下一个分隔符（如果不是最后一个条目）
                if (i < count - 1) {
                    char delimiter;
                    iss.get(delimiter);
                }
                
                args.entries.push_back(LogEntry::deserialize(entry_str));
            }
            
            return args;
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid AppendEntriesArgs format: " + 
                                      std::string(e.what()));
        }
    }
};

/**
 * @brief AppendEntries RPC 响应
 * 
 * Follower 对日志追加请求的响应。
 * 需求: 2.1.2 (日志复制), 2.3.1 (节点间 RPC 通信)
 */
struct AppendEntriesReply {
    int term;           // 当前任期号，用于 Leader 更新自己
    bool success;       // 如果 Follower 包含匹配 prevLogIndex 和 prevLogTerm 的条目则为 true
    int conflictIndex;  // 冲突日志的索引（用于快速回退优化）
    int conflictTerm;   // 冲突日志的任期号（用于快速回退优化）

    AppendEntriesReply()
        : term(0), success(false), conflictIndex(-1), conflictTerm(-1) {}

    AppendEntriesReply(int t, bool succ, int ci = -1, int ct = -1)
        : term(t), success(succ), conflictIndex(ci), conflictTerm(ct) {}

    /**
     * @brief 序列化为字符串
     * 格式: "term|success|conflictIndex|conflictTerm"
     */
    std::string serialize() const {
        std::ostringstream oss;
        oss << term << "|" << (success ? "1" : "0") << "|" 
            << conflictIndex << "|" << conflictTerm;
        return oss.str();
    }

    /**
     * @brief 从字符串反序列化
     */
    static AppendEntriesReply deserialize(const std::string& str) {
        std::istringstream iss(str);
        std::string term_str, success_str, ci_str, ct_str;
        
        if (!std::getline(iss, term_str, '|') ||
            !std::getline(iss, success_str, '|') ||
            !std::getline(iss, ci_str, '|') ||
            !std::getline(iss, ct_str)) {
            throw std::invalid_argument("Invalid AppendEntriesReply format");
        }

        try {
            return AppendEntriesReply(
                std::stoi(term_str),
                success_str == "1",
                std::stoi(ci_str),
                std::stoi(ct_str)
            );
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid AppendEntriesReply format: " + 
                                      std::string(e.what()));
        }
    }
};

} // namespace raft

#endif // RAFT_RPC_MESSAGES_H
