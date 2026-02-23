#include "storage/command.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace raft_kv;

void test_put_command() {
    std::cout << "Testing PUT command..." << std::endl;
    
    // 创建 PUT 命令
    Command cmd(CommandType::PUT, "name", "Alice");
    assert(cmd.getType() == CommandType::PUT);
    assert(cmd.getKey() == "name");
    assert(cmd.getValue() == "Alice");
    
    // 序列化
    std::string serialized = cmd.serialize();
    assert(serialized == "PUT:name:Alice");
    std::cout << "  Serialized: " << serialized << std::endl;
    
    // 反序列化
    Command deserialized = Command::deserialize(serialized);
    assert(deserialized.getType() == CommandType::PUT);
    assert(deserialized.getKey() == "name");
    assert(deserialized.getValue() == "Alice");
    
    std::cout << "  PUT command test passed!" << std::endl;
}

void test_get_command() {
    std::cout << "Testing GET command..." << std::endl;
    
    // 创建 GET 命令
    Command cmd(CommandType::GET, "name");
    assert(cmd.getType() == CommandType::GET);
    assert(cmd.getKey() == "name");
    assert(cmd.getValue() == "");
    
    // 序列化
    std::string serialized = cmd.serialize();
    assert(serialized == "GET:name");
    std::cout << "  Serialized: " << serialized << std::endl;
    
    // 反序列化
    Command deserialized = Command::deserialize(serialized);
    assert(deserialized.getType() == CommandType::GET);
    assert(deserialized.getKey() == "name");
    
    std::cout << "  GET command test passed!" << std::endl;
}

void test_delete_command() {
    std::cout << "Testing DELETE command..." << std::endl;
    
    // 创建 DELETE 命令
    Command cmd(CommandType::DELETE, "name");
    assert(cmd.getType() == CommandType::DELETE);
    assert(cmd.getKey() == "name");
    assert(cmd.getValue() == "");
    
    // 序列化
    std::string serialized = cmd.serialize();
    assert(serialized == "DELETE:name");
    std::cout << "  Serialized: " << serialized << std::endl;
    
    // 反序列化
    Command deserialized = Command::deserialize(serialized);
    assert(deserialized.getType() == CommandType::DELETE);
    assert(deserialized.getKey() == "name");
    
    std::cout << "  DELETE command test passed!" << std::endl;
}

void test_special_characters() {
    std::cout << "Testing special characters in values..." << std::endl;
    
    // 测试包含冒号的值
    Command cmd1(CommandType::PUT, "url", "http://example.com:8080");
    std::string serialized1 = cmd1.serialize();
    assert(serialized1 == "PUT:url:http://example.com:8080");
    
    Command deserialized1 = Command::deserialize(serialized1);
    assert(deserialized1.getKey() == "url");
    assert(deserialized1.getValue() == "http://example.com:8080");
    
    // 测试空值
    Command cmd2(CommandType::PUT, "empty", "");
    std::string serialized2 = cmd2.serialize();
    assert(serialized2 == "PUT:empty:");
    
    Command deserialized2 = Command::deserialize(serialized2);
    assert(deserialized2.getKey() == "empty");
    assert(deserialized2.getValue() == "");
    
    std::cout << "  Special characters test passed!" << std::endl;
}

void test_invalid_commands() {
    std::cout << "Testing invalid command handling..." << std::endl;
    
    // 测试空字符串
    try {
        Command::deserialize("");
        assert(false && "Should throw exception for empty string");
    } catch (const std::invalid_argument& e) {
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }
    
    // 测试无效的命令类型
    try {
        Command::deserialize("INVALID:key");
        assert(false && "Should throw exception for invalid command type");
    } catch (const std::invalid_argument& e) {
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }
    
    // 测试缺少冒号
    try {
        Command::deserialize("GETkey");
        assert(false && "Should throw exception for missing colon");
    } catch (const std::invalid_argument& e) {
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }
    
    // 测试 PUT 命令缺少值
    try {
        Command::deserialize("PUT:key");
        assert(false && "Should throw exception for PUT without value");
    } catch (const std::invalid_argument& e) {
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }
    
    // 测试空键
    try {
        Command::deserialize("GET:");
        assert(false && "Should throw exception for empty key");
    } catch (const std::invalid_argument& e) {
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }
    
    std::cout << "  Invalid command test passed!" << std::endl;
}

int main() {
    std::cout << "=== Command Test Suite ===" << std::endl << std::endl;
    
    test_put_command();
    std::cout << std::endl;
    
    test_get_command();
    std::cout << std::endl;
    
    test_delete_command();
    std::cout << std::endl;
    
    test_special_characters();
    std::cout << std::endl;
    
    test_invalid_commands();
    std::cout << std::endl;
    
    std::cout << "=== All Command Tests Passed! ===" << std::endl;
    
    return 0;
}
