#include "storage/persister.h"
#include "raft/log_entry.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define rmdir(path) _rmdir(path)
#else
#include <unistd.h>
#endif

// 辅助函数：删除目录及其内容
void removeDirectory(const std::string& path) {
    std::string stateFile = path + "/raft_state.dat";
    std::string tempFile = path + "/raft_state.dat.tmp";
    
    // 删除文件
    std::remove(stateFile.c_str());
    std::remove(tempFile.c_str());
    
    // 删除目录
    rmdir(path.c_str());
}

// 辅助函数：检查目录是否存在
bool directoryExists(const std::string& path) {
    struct stat info;
    return (stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR));
}

/**
 * @brief 测试 Persister 构造函数和目录创建
 */
void test_constructor_creates_directory() {
    std::cout << "Testing Persister constructor creates directory..." << std::endl;
    
    std::string testDir = "./test_data_persister_1";
    removeDirectory(testDir);
    
    assert(!directoryExists(testDir));
    
    storage::Persister persister(testDir);
    
    assert(directoryExists(testDir));
    
    removeDirectory(testDir);
    std::cout << "  Constructor test passed!" << std::endl;
}

/**
 * @brief 测试保存和加载空日志
 */
void test_save_and_load_empty_log() {
    std::cout << "Testing save and load empty log..." << std::endl;
    
    std::string testDir = "./test_data_persister_2";
    removeDirectory(testDir);
    
    storage::Persister persister(testDir);
    
    int currentTerm = 5;
    int votedFor = 2;
    std::vector<raft::LogEntry> log;
    
    // 保存状态
    assert(persister.saveRaftState(currentTerm, votedFor, log));
    assert(persister.exists());
    
    // 加载状态
    int loadedTerm = 0;
    int loadedVotedFor = 0;
    std::vector<raft::LogEntry> loadedLog;
    
    assert(persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
    assert(loadedTerm == currentTerm);
    assert(loadedVotedFor == votedFor);
    assert(loadedLog.empty());
    
    removeDirectory(testDir);
    std::cout << "  Empty log test passed!" << std::endl;
}

/**
 * @brief 测试保存和加载单个日志条目
 */
void test_save_and_load_single_log_entry() {
    std::cout << "Testing save and load single log entry..." << std::endl;
    
    std::string testDir = "./test_data_persister_3";
    removeDirectory(testDir);
    
    storage::Persister persister(testDir);
    
    int currentTerm = 3;
    int votedFor = 1;
    std::vector<raft::LogEntry> log;
    log.emplace_back(1, "PUT:key1:value1", 1);
    
    // 保存状态
    assert(persister.saveRaftState(currentTerm, votedFor, log));
    
    // 加载状态
    int loadedTerm = 0;
    int loadedVotedFor = 0;
    std::vector<raft::LogEntry> loadedLog;
    
    assert(persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
    assert(loadedTerm == currentTerm);
    assert(loadedVotedFor == votedFor);
    assert(loadedLog.size() == 1);
    assert(loadedLog[0].term == 1);
    assert(loadedLog[0].command == "PUT:key1:value1");
    assert(loadedLog[0].index == 1);
    
    removeDirectory(testDir);
    std::cout << "  Single log entry test passed!" << std::endl;
}

/**
 * @brief 测试保存和加载多个日志条目
 */
void test_save_and_load_multiple_log_entries() {
    std::cout << "Testing save and load multiple log entries..." << std::endl;
    
    std::string testDir = "./test_data_persister_4";
    removeDirectory(testDir);
    
    storage::Persister persister(testDir);
    
    int currentTerm = 7;
    int votedFor = 3;
    std::vector<raft::LogEntry> log;
    log.emplace_back(1, "PUT:key1:value1", 1);
    log.emplace_back(2, "PUT:key2:value2", 2);
    log.emplace_back(3, "DELETE:key1", 3);
    log.emplace_back(3, "GET:key2", 4);
    
    // 保存状态
    assert(persister.saveRaftState(currentTerm, votedFor, log));
    
    // 加载状态
    int loadedTerm = 0;
    int loadedVotedFor = 0;
    std::vector<raft::LogEntry> loadedLog;
    
    assert(persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
    assert(loadedTerm == currentTerm);
    assert(loadedVotedFor == votedFor);
    assert(loadedLog.size() == log.size());
    
    for (size_t i = 0; i < log.size(); ++i) {
        assert(loadedLog[i].term == log[i].term);
        assert(loadedLog[i].command == log[i].command);
        assert(loadedLog[i].index == log[i].index);
    }
    
    removeDirectory(testDir);
    std::cout << "  Multiple log entries test passed!" << std::endl;
}

/**
 * @brief 测试未投票状态（votedFor = -1）
 */
void test_save_and_load_no_vote() {
    std::cout << "Testing save and load no vote state..." << std::endl;
    
    std::string testDir = "./test_data_persister_5";
    removeDirectory(testDir);
    
    storage::Persister persister(testDir);
    
    int currentTerm = 1;
    int votedFor = -1;  // 未投票
    std::vector<raft::LogEntry> log;
    
    // 保存状态
    assert(persister.saveRaftState(currentTerm, votedFor, log));
    
    // 加载状态
    int loadedTerm = 0;
    int loadedVotedFor = 0;
    std::vector<raft::LogEntry> loadedLog;
    
    assert(persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
    assert(loadedTerm == currentTerm);
    assert(loadedVotedFor == -1);
    
    removeDirectory(testDir);
    std::cout << "  No vote test passed!" << std::endl;
}

/**
 * @brief 测试覆盖写入
 */
void test_overwrite_state() {
    std::cout << "Testing overwrite state..." << std::endl;
    
    std::string testDir = "./test_data_persister_6";
    removeDirectory(testDir);
    
    storage::Persister persister(testDir);
    
    // 第一次保存
    int term1 = 1;
    int vote1 = 1;
    std::vector<raft::LogEntry> log1;
    log1.emplace_back(1, "PUT:a:1", 1);
    assert(persister.saveRaftState(term1, vote1, log1));
    
    // 第二次保存（覆盖）
    int term2 = 5;
    int vote2 = 2;
    std::vector<raft::LogEntry> log2;
    log2.emplace_back(3, "PUT:b:2", 1);
    log2.emplace_back(4, "PUT:c:3", 2);
    assert(persister.saveRaftState(term2, vote2, log2));
    
    // 加载状态，应该是第二次保存的状态
    int loadedTerm = 0;
    int loadedVotedFor = 0;
    std::vector<raft::LogEntry> loadedLog;
    
    assert(persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
    assert(loadedTerm == term2);
    assert(loadedVotedFor == vote2);
    assert(loadedLog.size() == log2.size());
    assert(loadedLog[0].command == "PUT:b:2");
    assert(loadedLog[1].command == "PUT:c:3");
    
    removeDirectory(testDir);
    std::cout << "  Overwrite test passed!" << std::endl;
}

/**
 * @brief 测试加载不存在的文件
 */
void test_load_non_existent_file() {
    std::cout << "Testing load non-existent file..." << std::endl;
    
    std::string testDir = "./test_data_persister_7";
    removeDirectory(testDir);
    
    storage::Persister persister(testDir);
    
    assert(!persister.exists());
    
    int loadedTerm = 0;
    int loadedVotedFor = 0;
    std::vector<raft::LogEntry> loadedLog;
    
    assert(!persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
    
    removeDirectory(testDir);
    std::cout << "  Non-existent file test passed!" << std::endl;
}

/**
 * @brief 测试清除持久化数据
 */
void test_clear_state() {
    std::cout << "Testing clear state..." << std::endl;
    
    std::string testDir = "./test_data_persister_8";
    removeDirectory(testDir);
    
    storage::Persister persister(testDir);
    
    // 保存状态
    int currentTerm = 10;
    int votedFor = 5;
    std::vector<raft::LogEntry> log;
    log.emplace_back(8, "PUT:x:y", 1);
    assert(persister.saveRaftState(currentTerm, votedFor, log));
    assert(persister.exists());
    
    // 清除状态
    assert(persister.clear());
    assert(!persister.exists());
    
    // 尝试加载应该失败
    int loadedTerm = 0;
    int loadedVotedFor = 0;
    std::vector<raft::LogEntry> loadedLog;
    assert(!persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
    
    removeDirectory(testDir);
    std::cout << "  Clear state test passed!" << std::endl;
}

/**
 * @brief 测试包含特殊字符的命令
 */
void test_save_and_load_special_characters() {
    std::cout << "Testing save and load special characters..." << std::endl;
    
    std::string testDir = "./test_data_persister_9";
    removeDirectory(testDir);
    
    storage::Persister persister(testDir);
    
    int currentTerm = 2;
    int votedFor = 1;
    std::vector<raft::LogEntry> log;
    
    // 包含特殊字符的命令
    log.emplace_back(1, "PUT:key:value\nwith\nnewlines", 1);
    log.emplace_back(2, "PUT:key2:value|with|pipes", 2);
    log.emplace_back(2, "PUT:key3:", 3);  // 空值
    
    // 保存状态
    assert(persister.saveRaftState(currentTerm, votedFor, log));
    
    // 加载状态
    int loadedTerm = 0;
    int loadedVotedFor = 0;
    std::vector<raft::LogEntry> loadedLog;
    
    assert(persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
    assert(loadedLog.size() == log.size());
    
    for (size_t i = 0; i < log.size(); ++i) {
        assert(loadedLog[i].term == log[i].term);
        assert(loadedLog[i].command == log[i].command);
        assert(loadedLog[i].index == log[i].index);
    }
    
    removeDirectory(testDir);
    std::cout << "  Special characters test passed!" << std::endl;
}

/**
 * @brief 测试大量日志条目
 */
void test_save_and_load_large_log() {
    std::cout << "Testing save and load large log..." << std::endl;
    
    std::string testDir = "./test_data_persister_10";
    removeDirectory(testDir);
    
    storage::Persister persister(testDir);
    
    int currentTerm = 100;
    int votedFor = 50;
    std::vector<raft::LogEntry> log;
    
    // 创建 1000 个日志条目
    for (int i = 1; i <= 1000; ++i) {
        std::string command = "PUT:key" + std::to_string(i) + ":value" + std::to_string(i);
        log.emplace_back(i / 10 + 1, command, i);
    }
    
    // 保存状态
    assert(persister.saveRaftState(currentTerm, votedFor, log));
    
    // 加载状态
    int loadedTerm = 0;
    int loadedVotedFor = 0;
    std::vector<raft::LogEntry> loadedLog;
    
    assert(persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
    assert(loadedTerm == currentTerm);
    assert(loadedVotedFor == votedFor);
    assert(loadedLog.size() == log.size());
    
    // 验证前几个和后几个条目
    assert(loadedLog[0].command == "PUT:key1:value1");
    assert(loadedLog[999].command == "PUT:key1000:value1000");
    
    removeDirectory(testDir);
    std::cout << "  Large log test passed!" << std::endl;
}

/**
 * @brief 测试崩溃恢复场景
 */
void test_crash_recovery_scenario() {
    std::cout << "Testing crash recovery scenario..." << std::endl;
    
    std::string testDir = "./test_data_persister_11";
    removeDirectory(testDir);
    
    // 第一个 Persister 实例（模拟崩溃前）
    {
        storage::Persister persister(testDir);
        
        int currentTerm = 15;
        int votedFor = 3;
        std::vector<raft::LogEntry> log;
        log.emplace_back(10, "PUT:key1:value1", 1);
        log.emplace_back(12, "PUT:key2:value2", 2);
        log.emplace_back(15, "DELETE:key1", 3);
        
        assert(persister.saveRaftState(currentTerm, votedFor, log));
    }
    // Persister 析构（模拟崩溃）
    
    // 第二个 Persister 实例（模拟重启后）
    {
        storage::Persister persister(testDir);
        
        int loadedTerm = 0;
        int loadedVotedFor = 0;
        std::vector<raft::LogEntry> loadedLog;
        
        assert(persister.loadRaftState(loadedTerm, loadedVotedFor, loadedLog));
        assert(loadedTerm == 15);
        assert(loadedVotedFor == 3);
        assert(loadedLog.size() == 3);
        assert(loadedLog[0].command == "PUT:key1:value1");
        assert(loadedLog[1].command == "PUT:key2:value2");
        assert(loadedLog[2].command == "DELETE:key1");
    }
    
    removeDirectory(testDir);
    std::cout << "  Crash recovery test passed!" << std::endl;
}

int main() {
    std::cout << "=== Persister Tests ===" << std::endl;
    
    try {
        test_constructor_creates_directory();
        test_save_and_load_empty_log();
        test_save_and_load_single_log_entry();
        test_save_and_load_multiple_log_entries();
        test_save_and_load_no_vote();
        test_overwrite_state();
        test_load_non_existent_file();
        test_clear_state();
        test_save_and_load_special_characters();
        test_save_and_load_large_log();
        test_crash_recovery_scenario();
        
        std::cout << "\n=== All Persister Tests Passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
