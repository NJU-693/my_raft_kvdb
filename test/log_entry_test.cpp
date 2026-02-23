#include "../include/raft/log_entry.h"
#include <cassert>
#include <iostream>

using namespace raft;

void test_log_entry_construction() {
    std::cout << "Testing LogEntry construction..." << std::endl;
    
    // 默认构造
    LogEntry entry1;
    assert(entry1.term == 0);
    assert(entry1.command == "");
    assert(entry1.index == 0);
    
    // 参数构造
    LogEntry entry2(1, "PUT:key1:value1", 1);
    assert(entry2.term == 1);
    assert(entry2.command == "PUT:key1:value1");
    assert(entry2.index == 1);
    
    std::cout << "✓ LogEntry construction test passed" << std::endl;
}

void test_log_entry_serialization() {
    std::cout << "Testing LogEntry serialization..." << std::endl;
    
    LogEntry entry(5, "DELETE:key2", 10);
    std::string serialized = entry.serialize();
    
    // 验证格式: "term|index|length|command"
    assert(serialized == "5|10|11|DELETE:key2");
    
    std::cout << "✓ LogEntry serialization test passed" << std::endl;
}

void test_log_entry_deserialization() {
    std::cout << "Testing LogEntry deserialization..." << std::endl;
    
    std::string serialized = "3|7|9|GET:mykey";
    LogEntry entry = LogEntry::deserialize(serialized);
    
    assert(entry.term == 3);
    assert(entry.index == 7);
    assert(entry.command == "GET:mykey");
    
    std::cout << "✓ LogEntry deserialization test passed" << std::endl;
}

void test_log_entry_round_trip() {
    std::cout << "Testing LogEntry round-trip..." << std::endl;
    
    LogEntry original(2, "PUT:name:Alice", 5);
    std::string serialized = original.serialize();
    LogEntry deserialized = LogEntry::deserialize(serialized);
    
    assert(original == deserialized);
    assert(original.term == deserialized.term);
    assert(original.command == deserialized.command);
    assert(original.index == deserialized.index);
    
    std::cout << "✓ LogEntry round-trip test passed" << std::endl;
}

void test_log_entry_with_special_characters() {
    std::cout << "Testing LogEntry with special characters..." << std::endl;
    
    // 测试包含特殊字符的命令
    LogEntry entry(1, "PUT:key:value with spaces and |pipes|", 1);
    std::string serialized = entry.serialize();
    LogEntry deserialized = LogEntry::deserialize(serialized);
    
    assert(entry == deserialized);
    
    std::cout << "✓ LogEntry special characters test passed" << std::endl;
}

void test_log_entry_invalid_format() {
    std::cout << "Testing LogEntry invalid format..." << std::endl;
    
    bool caught = false;
    try {
        LogEntry::deserialize("invalid");
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);
    
    caught = false;
    try {
        LogEntry::deserialize("1|2|3");  // 缺少 command
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);
    
    caught = false;
    try {
        LogEntry::deserialize("1|2|10|short");  // 长度不匹配
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);
    
    std::cout << "✓ LogEntry invalid format test passed" << std::endl;
}

void test_log_entry_equality() {
    std::cout << "Testing LogEntry equality..." << std::endl;
    
    LogEntry entry1(1, "PUT:a:b", 1);
    LogEntry entry2(1, "PUT:a:b", 1);
    LogEntry entry3(2, "PUT:a:b", 1);
    
    assert(entry1 == entry2);
    assert(entry1 != entry3);
    
    std::cout << "✓ LogEntry equality test passed" << std::endl;
}

int main() {
    std::cout << "Running LogEntry tests..." << std::endl;
    std::cout << "================================" << std::endl;
    
    test_log_entry_construction();
    test_log_entry_serialization();
    test_log_entry_deserialization();
    test_log_entry_round_trip();
    test_log_entry_with_special_characters();
    test_log_entry_invalid_format();
    test_log_entry_equality();
    
    std::cout << "================================" << std::endl;
    std::cout << "All LogEntry tests passed!" << std::endl;
    
    return 0;
}
