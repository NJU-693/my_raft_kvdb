#include "storage/skiplist.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace raft_kv;

// 测试跳表节点的基本功能
void testSkipListNodeCreation() {
    // 测试整数键值对
    SkipListNode<int, std::string> node1(42, "value42", 3);
    assert(node1.key == 42);
    assert(node1.value == "value42");
    assert(node1.forward.size() == 4); // level 3 means 4 layers (0-3)
    
    // 测试字符串键值对
    SkipListNode<std::string, int> node2("key", 100, 0);
    assert(node2.key == "key");
    assert(node2.value == 100);
    assert(node2.forward.size() == 1); // level 0 means 1 layer
    
    std::cout << "✓ Node creation test passed" << std::endl;
}

// 测试前向指针初始化
void testForwardPointers() {
    SkipListNode<int, int> node(10, 20, 5);
    
    // 验证所有前向指针初始化为 nullptr
    for (size_t i = 0; i < node.forward.size(); ++i) {
        assert(node.forward[i] == nullptr);
    }
    
    std::cout << "✓ Forward pointers initialization test passed" << std::endl;
}

// 测试前向指针设置
void testForwardPointersSetting() {
    SkipListNode<int, int> node1(10, 100, 2);
    SkipListNode<int, int> node2(20, 200, 2);
    SkipListNode<int, int> node3(30, 300, 2);
    
    // 设置前向指针
    node1.forward[0] = &node2;
    node1.forward[1] = &node2;
    node1.forward[2] = &node3;
    
    // 验证指针设置正确
    assert(node1.forward[0] == &node2);
    assert(node1.forward[1] == &node2);
    assert(node1.forward[2] == &node3);
    
    // 验证可以通过指针访问节点数据
    assert(node1.forward[0]->key == 20);
    assert(node1.forward[0]->value == 200);
    
    std::cout << "✓ Forward pointers setting test passed" << std::endl;
}

int main() {
    std::cout << "Running SkipListNode tests..." << std::endl;
    
    testSkipListNodeCreation();
    testForwardPointers();
    testForwardPointersSetting();
    
    std::cout << "\nAll tests passed! ✓" << std::endl;
    return 0;
}
