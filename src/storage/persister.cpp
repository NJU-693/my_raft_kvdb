#include "storage/persister.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

namespace storage {

Persister::Persister(const std::string& dataDir) 
    : dataDir_(dataDir),
      stateFile_(dataDir + "/raft_state.dat"),
      tempFile_(dataDir + "/raft_state.dat.tmp") {
    ensureDataDir();
}

bool Persister::ensureDataDir() {
    struct stat info;
    
    // 检查目录是否存在
    if (stat(dataDir_.c_str(), &info) == 0) {
        if (info.st_mode & S_IFDIR) {
            return true;  // 目录已存在
        } else {
            return false;  // 路径存在但不是目录
        }
    }
    
    // 目录不存在，创建它
    if (mkdir(dataDir_.c_str(), 0755) == 0) {
        return true;
    }
    
    // 创建失败，检查是否是因为目录已存在
    if (errno == EEXIST) {
        return true;
    }
    
    return false;
}

bool Persister::saveRaftState(int currentTerm, int votedFor,
                              const std::vector<raft::LogEntry>& log) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 确保数据目录存在
    if (!ensureDataDir()) {
        return false;
    }
    
    // 序列化状态
    std::string data = serializeState(currentTerm, votedFor, log);
    
    // 先写入临时文件
    std::ofstream tempOut(tempFile_, std::ios::binary | std::ios::trunc);
    if (!tempOut.is_open()) {
        return false;
    }
    
    tempOut << data;
    tempOut.flush();
    
    if (!tempOut.good()) {
        tempOut.close();
        return false;
    }
    
    tempOut.close();
    
    // 原子性重命名：将临时文件重命名为正式文件
    // 在 POSIX 系统上，rename 是原子操作
    if (std::rename(tempFile_.c_str(), stateFile_.c_str()) != 0) {
        return false;
    }
    
    return true;
}

bool Persister::loadRaftState(int& currentTerm, int& votedFor,
                              std::vector<raft::LogEntry>& log) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查文件是否存在
    std::ifstream in(stateFile_, std::ios::binary);
    if (!in.is_open()) {
        return false;  // 文件不存在或无法打开
    }
    
    // 读取整个文件内容
    std::stringstream buffer;
    buffer << in.rdbuf();
    in.close();
    
    std::string data = buffer.str();
    if (data.empty()) {
        return false;  // 文件为空
    }
    
    // 反序列化状态
    return deserializeState(data, currentTerm, votedFor, log);
}

bool Persister::exists() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ifstream in(stateFile_);
    return in.good();
}

bool Persister::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 删除状态文件
    if (std::remove(stateFile_.c_str()) != 0 && errno != ENOENT) {
        return false;
    }
    
    // 删除临时文件（如果存在）
    std::remove(tempFile_.c_str());  // 忽略错误
    
    return true;
}

std::string Persister::serializeState(int currentTerm, int votedFor,
                                      const std::vector<raft::LogEntry>& log) const {
    std::ostringstream oss;
    
    // Line 1: currentTerm
    oss << currentTerm << "\n";
    
    // Line 2: votedFor
    oss << votedFor << "\n";
    
    // Line 3: log_count
    oss << log.size() << "\n";
    
    // Line 4+: 每个日志条目（使用长度前缀格式）
    for (const auto& entry : log) {
        std::string serialized = entry.serialize();
        oss << serialized.length() << "\n";  // 长度
        oss << serialized;                    // 内容（不加换行符）
    }
    
    return oss.str();
}

bool Persister::deserializeState(const std::string& data, int& currentTerm,
                                 int& votedFor, std::vector<raft::LogEntry>& log) const {
    std::istringstream iss(data);
    std::string line;
    
    // Line 1: currentTerm
    if (!std::getline(iss, line)) {
        return false;
    }
    try {
        currentTerm = std::stoi(line);
    } catch (...) {
        return false;
    }
    
    // Line 2: votedFor
    if (!std::getline(iss, line)) {
        return false;
    }
    try {
        votedFor = std::stoi(line);
    } catch (...) {
        return false;
    }
    
    // Line 3: log_count
    if (!std::getline(iss, line)) {
        return false;
    }
    size_t logCount;
    try {
        logCount = std::stoull(line);
    } catch (...) {
        return false;
    }
    
    // Line 4+: 日志条目（使用长度前缀格式）
    log.clear();
    log.reserve(logCount);
    
    for (size_t i = 0; i < logCount; ++i) {
        // 读取长度
        if (!std::getline(iss, line)) {
            return false;
        }
        size_t length;
        try {
            length = std::stoull(line);
        } catch (...) {
            return false;
        }
        
        // 读取指定长度的内容
        std::string serialized(length, '\0');
        iss.read(&serialized[0], length);
        
        if (iss.gcount() != static_cast<std::streamsize>(length)) {
            return false;  // 读取的字节数不匹配
        }
        
        try {
            raft::LogEntry entry = raft::LogEntry::deserialize(serialized);
            log.push_back(entry);
        } catch (...) {
            return false;  // 日志条目格式错误
        }
    }
    
    return true;
}

} // namespace storage
