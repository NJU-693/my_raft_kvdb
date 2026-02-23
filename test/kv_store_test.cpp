#include "storage/kv_store.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>

using namespace raft_kv;

// 测试基本的 Put 操作
void test_put_operation() {
    std::cout << "Testing Put operation..." << std::endl;
    
    KVStore store;
    
    // 插入新键
    bool result1 = store.put("key1", "value1");
    assert(result1 == true);  // 新键插入应返回 true
    assert(store.size() == 1);
    
    // 更新已有键
    bool result2 = store.put("key1", "value2");
    assert(result2 == false);  // 更新已有键应返回 false
    assert(store.size() == 1);  // 大小不变
    
    // 插入多个键
    store.put("key2", "value2");
    store.put("key3", "value3");
    assert(store.size() == 3);
    
    std::cout << "  Put operation test passed!" << std::endl;
}

// 测试基本的 Get 操作
void test_get_operation() {
    std::cout << "Testing Get operation..." << std::endl;
    
    KVStore store;
    
    // 插入数据
    store.put("name", "Alice");
    store.put("age", "25");
    store.put("city", "Beijing");
    
    // 获取存在的键
    std::string value;
    bool result1 = store.get("name", value);
    assert(result1 == true);
    assert(value == "Alice");
    
    result1 = store.get("age", value);
    assert(result1 == true);
    assert(value == "25");
    
    // 获取不存在的键
    bool result2 = store.get("nonexistent", value);
    assert(result2 == false);
    
    std::cout << "  Get operation test passed!" << std::endl;
}

// 测试基本的 Delete 操作
void test_delete_operation() {
    std::cout << "Testing Delete operation..." << std::endl;
    
    KVStore store;
    
    // 插入数据
    store.put("key1", "value1");
    store.put("key2", "value2");
    store.put("key3", "value3");
    assert(store.size() == 3);
    
    // 删除存在的键
    bool result1 = store.remove("key2");
    assert(result1 == true);
    assert(store.size() == 2);
    
    // 验证删除后无法获取
    std::string value;
    bool result2 = store.get("key2", value);
    assert(result2 == false);
    
    // 删除不存在的键
    bool result3 = store.remove("nonexistent");
    assert(result3 == false);
    assert(store.size() == 2);
    
    // 删除所有键
    store.remove("key1");
    store.remove("key3");
    assert(store.size() == 0);
    
    std::cout << "  Delete operation test passed!" << std::endl;
}

// 测试 Put/Get/Delete 组合操作
void test_combined_operations() {
    std::cout << "Testing combined operations..." << std::endl;
    
    KVStore store;
    
    // 插入、更新、查询、删除的组合
    store.put("user", "Alice");
    
    std::string value;
    store.get("user", value);
    assert(value == "Alice");
    
    store.put("user", "Bob");
    store.get("user", value);
    assert(value == "Bob");
    
    store.remove("user");
    bool result = store.get("user", value);
    assert(result == false);
    
    // 删除后重新插入
    store.put("user", "Charlie");
    store.get("user", value);
    assert(value == "Charlie");
    
    std::cout << "  Combined operations test passed!" << std::endl;
}

// 测试命令应用接口 - PUT 命令
void test_apply_put_command() {
    std::cout << "Testing apply PUT command..." << std::endl;
    
    KVStore store;
    
    // 应用 PUT 命令
    store.applyCommand("PUT:name:Alice");
    store.applyCommand("PUT:age:25");
    
    // 验证结果
    std::string value;
    assert(store.get("name", value) == true);
    assert(value == "Alice");
    
    assert(store.get("age", value) == true);
    assert(value == "25");
    
    assert(store.size() == 2);
    
    // 应用更新命令
    store.applyCommand("PUT:name:Bob");
    assert(store.get("name", value) == true);
    assert(value == "Bob");
    assert(store.size() == 2);  // 大小不变
    
    std::cout << "  Apply PUT command test passed!" << std::endl;
}

// 测试命令应用接口 - DELETE 命令
void test_apply_delete_command() {
    std::cout << "Testing apply DELETE command..." << std::endl;
    
    KVStore store;
    
    // 先插入数据
    store.put("key1", "value1");
    store.put("key2", "value2");
    assert(store.size() == 2);
    
    // 应用 DELETE 命令
    store.applyCommand("DELETE:key1");
    
    // 验证删除结果
    std::string value;
    assert(store.get("key1", value) == false);
    assert(store.size() == 1);
    
    // 删除不存在的键（不应抛出异常）
    store.applyCommand("DELETE:nonexistent");
    assert(store.size() == 1);
    
    std::cout << "  Apply DELETE command test passed!" << std::endl;
}

// 测试命令应用接口 - GET 命令
void test_apply_get_command() {
    std::cout << "Testing apply GET command..." << std::endl;
    
    KVStore store;
    
    // 先插入数据
    store.put("key1", "value1");
    
    // 应用 GET 命令（不应修改状态）
    store.applyCommand("GET:key1");
    
    // 验证数据未被修改
    std::string value;
    assert(store.get("key1", value) == true);
    assert(value == "value1");
    assert(store.size() == 1);
    
    // GET 不存在的键（不应抛出异常）
    store.applyCommand("GET:nonexistent");
    assert(store.size() == 1);
    
    std::cout << "  Apply GET command test passed!" << std::endl;
}

// 测试命令应用接口 - 特殊字符
void test_apply_command_special_chars() {
    std::cout << "Testing apply command with special characters..." << std::endl;
    
    KVStore store;
    
    // 测试包含冒号的值
    store.applyCommand("PUT:url:http://example.com:8080");
    
    std::string value;
    assert(store.get("url", value) == true);
    assert(value == "http://example.com:8080");
    
    // 测试空值
    store.applyCommand("PUT:empty:");
    assert(store.get("empty", value) == true);
    assert(value == "");
    
    std::cout << "  Apply command with special characters test passed!" << std::endl;
}

// 测试并发 Put 操作
void test_concurrent_put() {
    std::cout << "Testing concurrent Put operations..." << std::endl;
    
    KVStore store;
    const int num_threads = 10;
    const int ops_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    // 每个线程插入不同的键
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&store, i, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                std::string value = "value_" + std::to_string(i) + "_" + std::to_string(j);
                store.put(key, value);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有数据都被正确插入
    assert(store.size() == num_threads * ops_per_thread);
    
    // 验证数据正确性
    for (int i = 0; i < num_threads; ++i) {
        for (int j = 0; j < ops_per_thread; ++j) {
            std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
            std::string expected_value = "value_" + std::to_string(i) + "_" + std::to_string(j);
            std::string actual_value;
            assert(store.get(key, actual_value) == true);
            assert(actual_value == expected_value);
        }
    }
    
    std::cout << "  Concurrent Put test passed!" << std::endl;
}

// 测试并发 Get 操作
void test_concurrent_get() {
    std::cout << "Testing concurrent Get operations..." << std::endl;
    
    KVStore store;
    const int num_keys = 100;
    const int num_threads = 10;
    const int reads_per_thread = 100;
    
    // 先插入数据
    for (int i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        store.put(key, value);
    }
    
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    
    // 多个线程并发读取
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&store, &success_count, num_keys, reads_per_thread]() {
            for (int j = 0; j < reads_per_thread; ++j) {
                int key_idx = j % num_keys;
                std::string key = "key_" + std::to_string(key_idx);
                std::string expected_value = "value_" + std::to_string(key_idx);
                std::string actual_value;
                
                if (store.get(key, actual_value) && actual_value == expected_value) {
                    success_count++;
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有读取都成功
    assert(success_count == num_threads * reads_per_thread);
    
    std::cout << "  Concurrent Get test passed!" << std::endl;
}

// 测试并发混合操作（Put/Get/Delete）
void test_concurrent_mixed_operations() {
    std::cout << "Testing concurrent mixed operations..." << std::endl;
    
    KVStore store;
    const int num_threads = 8;
    const int ops_per_thread = 50;
    
    std::vector<std::thread> threads;
    
    // 线程 1-3: Put 操作
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&store, i, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i * ops_per_thread + j);
                std::string value = "value_" + std::to_string(i * ops_per_thread + j);
                store.put(key, value);
            }
        });
    }
    
    // 线程 4-6: Get 操作
    for (int i = 3; i < 6; ++i) {
        threads.emplace_back([&store, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key = "key_" + std::to_string(j);
                std::string value;
                store.get(key, value);  // 可能成功也可能失败
            }
        });
    }
    
    // 线程 7-8: Delete 操作
    for (int i = 6; i < num_threads; ++i) {
        threads.emplace_back([&store, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key = "key_" + std::to_string(j);
                store.remove(key);  // 可能成功也可能失败
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证存储仍然可用（不崩溃即可）
    std::string value;
    store.get("key_0", value);  // 不关心结果，只要不崩溃
    
    std::cout << "  Concurrent mixed operations test passed!" << std::endl;
}

// 测试并发命令应用
void test_concurrent_apply_command() {
    std::cout << "Testing concurrent apply command..." << std::endl;
    
    KVStore store;
    const int num_threads = 10;
    const int ops_per_thread = 50;
    
    std::vector<std::thread> threads;
    
    // 每个线程应用不同的命令
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&store, i, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                std::string value = "value_" + std::to_string(i) + "_" + std::to_string(j);
                std::string command = "PUT:" + key + ":" + value;
                store.applyCommand(command);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证所有数据都被正确应用
    assert(store.size() == num_threads * ops_per_thread);
    
    std::cout << "  Concurrent apply command test passed!" << std::endl;
}

int main() {
    std::cout << "=== KV Store Test Suite ===" << std::endl << std::endl;
    
    // 基本操作测试
    test_put_operation();
    std::cout << std::endl;
    
    test_get_operation();
    std::cout << std::endl;
    
    test_delete_operation();
    std::cout << std::endl;
    
    test_combined_operations();
    std::cout << std::endl;
    
    // 命令应用接口测试
    test_apply_put_command();
    std::cout << std::endl;
    
    test_apply_delete_command();
    std::cout << std::endl;
    
    test_apply_get_command();
    std::cout << std::endl;
    
    test_apply_command_special_chars();
    std::cout << std::endl;
    
    // 并发测试
    test_concurrent_put();
    std::cout << std::endl;
    
    test_concurrent_get();
    std::cout << std::endl;
    
    test_concurrent_mixed_operations();
    std::cout << std::endl;
    
    test_concurrent_apply_command();
    std::cout << std::endl;
    
    std::cout << "=== All KV Store Tests Passed! ===" << std::endl;
    
    return 0;
}
