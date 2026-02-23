#include "storage/skiplist.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace raft_kv;

/**
 * 跳表单元测试
 * 
 * 测试内容：
 * 1. 基本插入和查找
 * 2. 更新已有键
 * 3. 删除操作
 * 4. 边界情况
 * 5. 大量数据测试
 */

// 测试基本插入和查找
void testBasicInsertAndSearch() {
    std::cout << "Test 1: Basic Insert and Search" << std::endl;
    
    SkipList<int, std::string> list;
    
    // 插入数据
    assert(list.insert(1, "one") == true);
    assert(list.insert(2, "two") == true);
    assert(list.insert(3, "three") == true);
    
    // 查找数据
    std::string value;
    assert(list.search(1, value) == true && value == "one");
    assert(list.search(2, value) == true && value == "two");
    assert(list.search(3, value) == true && value == "three");
    
    // 查找不存在的键
    assert(list.search(4, value) == false);
    
    assert(list.getSize() == 3);
    
    std::cout << "✓ Basic insert and search passed" << std::endl;
    list.display();
}

// 测试更新操作
void testUpdate() {
    std::cout << "\nTest 2: Update Existing Key" << std::endl;
    
    SkipList<int, std::string> list;
    
    list.insert(1, "one");
    list.insert(2, "two");
    
    // 更新已有键
    assert(list.insert(1, "ONE") == false);  // 返回 false 表示更新
    
    std::string value;
    assert(list.search(1, value) == true && value == "ONE");
    
    // 大小不变
    assert(list.getSize() == 2);
    
    std::cout << "✓ Update test passed" << std::endl;
    list.display();
}

// 测试删除操作
void testDelete() {
    std::cout << "\nTest 3: Delete Operation" << std::endl;
    
    SkipList<int, std::string> list;
    
    // 插入数据
    for (int i = 1; i <= 5; i++) {
        list.insert(i, "value" + std::to_string(i));
    }
    
    list.display();
    
    // TODO: 完成 remove 函数后，取消下面的注释进行测试
    
    // 删除中间元素
    assert(list.remove(3) == true);
    assert(list.getSize() == 4);
    
    std::string value;
    assert(list.search(3, value) == false);
    
    // 删除头部元素
    assert(list.remove(1) == true);
    assert(list.getSize() == 3);
    
    // 删除尾部元素
    assert(list.remove(5) == true);
    assert(list.getSize() == 2);
    
    // 删除不存在的元素
    assert(list.remove(10) == false);
    
    list.display();
    std::cout << "✓ Delete test passed" << std::endl;
    
    
    std::cout << "⚠ Delete test skipped (TODO: implement remove function)" << std::endl;
}

// 测试边界情况
void testEdgeCases() {
    std::cout << "\nTest 4: Edge Cases" << std::endl;
    
    SkipList<int, int> list;
    
    // 空跳表查找
    int value;
    assert(list.search(1, value) == false);
    assert(list.getSize() == 0);
    
    // 单个元素
    list.insert(1, 100);
    assert(list.search(1, value) == true && value == 100);
    assert(list.getSize() == 1);
    
    // TODO: 完成 remove 函数后测试删除唯一元素
    assert(list.remove(1) == true);
    assert(list.getSize() == 0);
    assert(list.search(1, value) == false);
    
    
    std::cout << "✓ Edge cases test passed" << std::endl;
}

// 测试大量数据
void testLargeData() {
    std::cout << "\nTest 5: Large Data Test" << std::endl;
    
    SkipList<int, int> list;
    const int N = 1000;
    
    // 插入大量数据
    for (int i = 0; i < N; i++) {
        list.insert(i, i * 10);
    }
    
    assert(list.getSize() == N);
    
    // 验证所有数据
    int value;
    for (int i = 0; i < N; i++) {
        assert(list.search(i, value) == true);
        assert(value == i * 10);
    }
    
    // TODO: 完成 remove 函数后测试删除

    // 删除一半数据
    for (int i = 0; i < N; i += 2) {
        assert(list.remove(i) == true);
    }
    
    assert(list.getSize() == N / 2);
    
    // 验证删除后的数据
    for (int i = 0; i < N; i++) {
        if (i % 2 == 0) {
            assert(list.search(i, value) == false);
        } else {
            assert(list.search(i, value) == true);
        }
    }

    
    std::cout << "✓ Large data test passed (inserted " << N << " elements)" << std::endl;
}

// 测试字符串键
void testStringKey() {
    std::cout << "\nTest 6: String Key Test" << std::endl;
    
    SkipList<std::string, int> list;
    
    list.insert("apple", 1);
    list.insert("banana", 2);
    list.insert("cherry", 3);
    list.insert("date", 4);
    
    int value;
    assert(list.search("apple", value) == true && value == 1);
    assert(list.search("banana", value) == true && value == 2);
    assert(list.search("cherry", value) == true && value == 3);
    assert(list.search("date", value) == true && value == 4);
    assert(list.search("elderberry", value) == false);
    
    list.display();
    std::cout << "✓ String key test passed" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    Skip List Unit Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        testBasicInsertAndSearch();
        testUpdate();
        testDelete();
        testEdgeCases();
        testLargeData();
        testStringKey();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  All tests passed! ✓" << std::endl;
        std::cout << "========================================" << std::endl;
        
        std::cout << "\n📝 TODO for you:" << std::endl;
        std::cout << "1. 完成 skiplist.h 中的 insert() 函数" << std::endl;
        std::cout << "2. 完成 skiplist.h 中的 remove() 函数" << std::endl;
        std::cout << "3. 取消本文件中被注释的删除测试" << std::endl;
        std::cout << "4. 重新编译并运行测试" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
