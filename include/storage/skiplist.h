#ifndef RAFT_KV_STORE_SKIPLIST_H
#define RAFT_KV_STORE_SKIPLIST_H

#include <iostream>
#include <random>
#include <vector>

/**
 * 跳表 (Skip List) 实现
 * 
 * 跳表是一种基于链表的数据结构，通过在链表上建立多层索引来实现快速查找。
 * 
 * 核心思想：
 * - 最底层（第0层）是一个有序链表，包含所有元素
 * - 上层是下层的"快速通道"，跳过一些元素
 * - 查找时从最高层开始，逐层下降，类似二分查找
 * 
 * 时间复杂度：
 * - 查找：O(log n)
 * - 插入：O(log n)
 * - 删除：O(log n)
 * 
 * 空间复杂度：O(n)
 * 
 * 示例结构（4层跳表）：
 * Level 3: 1 --------------------------------> 9
 * Level 2: 1 ------------> 5 ----------------> 9
 * Level 1: 1 ------> 3 --> 5 -----> 7 -------> 9
 * Level 0: 1 -> 2 -> 3 -> 5 -> 6 -> 7 -> 8 --> 9  (完整链表)
 */

namespace raft_kv {

/**
 * 跳表节点
 * 
 * 每个节点包含：
 * - key: 键（用于排序和查找）
 * - value: 值
 * - forward: 指针数组，forward[i] 指向第 i 层的下一个节点
 */
template <typename K, typename V>
struct SkipListNode {
    K key;
    V value;
    std::vector<SkipListNode*> forward;  // 各层的前向指针

    // 构造函数：创建一个具有指定层数的节点
    SkipListNode(K k, V v, int level) : key(k), value(v), forward(level + 1, nullptr) {}
};

/**
 * 跳表类
 * 
 * 主要操作：
 * - insert: 插入或更新键值对
 * - search: 查找键对应的值
 * - remove: 删除键值对
 */
template <typename K, typename V>
class SkipList {
private:
    static constexpr int MAX_LEVEL = 16;       // 最大层数（支持 2^16 = 65536 个元素）
    static constexpr float PROBABILITY = 0.5;  // 晋升到上一层的概率

    SkipListNode<K, V>* header;  // 头节点（哨兵节点，不存储实际数据）
    int currentLevel;            // 当前跳表的最高层数
    int size;                    // 元素数量
    std::mt19937 rng;            // 随机数生成器

    /**
     * 生成随机层数
     * 
     * 原理：
     * - 从第 0 层开始
     * - 以概率 PROBABILITY 晋升到下一层
     * - 最高不超过 MAX_LEVEL
     * 
     * 期望层数：log₂(n)
     * 
     * 例如：PROBABILITY = 0.5 时
     * - 50% 的节点只在第 0 层
     * - 25% 的节点在第 0-1 层
     * - 12.5% 的节点在第 0-2 层
     * - ...
     */
    int randomLevel() {
        int level = 0;
        std::uniform_real_distribution<> dis(0.0, 1.0);
        while (dis(rng) < PROBABILITY && level < MAX_LEVEL - 1) {
            level++;
        }
        return level;
    }

public:
    /**
     * 构造函数
     * 初始化跳表，创建头节点
     */
    SkipList() : currentLevel(0), size(0) {
        // 创建头节点，初始化为最大层数
        header = new SkipListNode<K, V>(K(), V(), MAX_LEVEL);
        
        // 初始化随机数生成器
        std::random_device rd;
        rng.seed(rd());
    }

    /**
     * 析构函数
     * 释放所有节点的内存
     */
    ~SkipList() {
        // 从第 0 层遍历，删除所有节点
        SkipListNode<K, V>* current = header->forward[0];
        while (current != nullptr) {
            SkipListNode<K, V>* next = current->forward[0];
            delete current;
            current = next;
        }
        delete header;
    }

    /**
     * 查找操作
     * 
     * 算法流程：
     * 1. 从最高层的头节点开始
     * 2. 在当前层向右移动，直到下一个节点的 key >= 目标 key
     * 3. 下降到下一层
     * 4. 重复步骤 2-3，直到第 0 层
     * 5. 检查找到的节点是否匹配
     * 
     * @param key 要查找的键
     * @param value 输出参数，存储找到的值
     * @return 是否找到
     */
    bool search(const K& key, V& value) {
        SkipListNode<K, V>* current = header;

        // 从最高层开始向下查找
        for (int level = currentLevel; level >= 0; level--) {
            // 在当前层向右移动，直到下一个节点的 key >= 目标 key
            while (current->forward[level] != nullptr && 
                   current->forward[level]->key < key) {
                current = current->forward[level];
            }
        }

        // 移动到第 0 层的下一个节点
        current = current->forward[0];

        // 检查是否找到目标节点
        if (current != nullptr && current->key == key) {
            value = current->value;
            return true;
        }

        return false;
    }

    /**
     * 插入操作
     * 
     * 算法流程：
     * 1. 查找插入位置，记录每层的前驱节点（update 数组）
     * 2. 如果 key 已存在，更新 value
     * 3. 否则，生成随机层数，创建新节点
     * 4. 更新各层的指针
     * 
     * @param key 键
     * @param value 值
     * @return 是否成功插入（false 表示更新已有键）
     */
    bool insert(const K& key, const V& value) {
        // update[i] 记录第 i 层中，key 应该插入位置的前一个节点
        std::vector<SkipListNode<K, V>*> update(MAX_LEVEL, nullptr);
        SkipListNode<K, V>* current = header;

        // TODO: 实现查找插入位置的逻辑
        // 提示：
        // 1. 从最高层开始向下遍历
        // 2. 在每一层，向右移动直到下一个节点的 key >= 目标 key
        // 3. 记录每层的前驱节点到 update 数组
        // 4. 下降到下一层，重复上述过程
        
        // 你的代码：
        for (int level = currentLevel; level >= 0; level--) {
            while (current->forward[level] != nullptr && 
                   current->forward[level]->key < key) {
                current = current->forward[level];
            }
            update[level] = current;
        }
        

        // 移动到第 0 层的下一个节点
        current = current->forward[0];

        // 如果 key 已存在，更新 value
        if (current != nullptr && current->key == key) {
            current->value = value;
            return false;  // 表示更新而非插入
        }

        // TODO: 实现插入新节点的逻辑
        // 提示：
        // 1. 生成随机层数：int newLevel = randomLevel();
        // 2. 如果新层数大于当前最高层，更新 update 数组和 currentLevel
        // 3. 创建新节点
        // 4. 更新各层的指针（从第 0 层到 newLevel）
        
        // 你的代码：
        int newLevel = randomLevel();
        if (newLevel > currentLevel) {
            for (int i = currentLevel + 1; i <= newLevel; i++) {
                update[i] = header;
            }
            currentLevel = newLevel;
        }
        SkipListNode<K, V>* newNode = new SkipListNode<K, V>(key, value, newLevel);
        for (int level = 0; level <= newLevel; level++) {
            newNode->forward[level] = update[level]->forward[level];
            update[level]->forward[level] = newNode;
        }

        size++;
        return true;
    }

    /**
     * 删除操作
     * 
     * 算法流程：
     * 1. 查找目标节点，记录每层的前驱节点
     * 2. 如果找到，更新各层的指针
     * 3. 删除节点
     * 4. 更新 currentLevel（如果最高层变空）
     * 
     * @param key 要删除的键
     * @return 是否成功删除
     */
    bool remove(const K& key) {
        std::vector<SkipListNode<K, V>*> update(MAX_LEVEL, nullptr);
        SkipListNode<K, V>* current = header;

        // TODO: 实现查找目标节点的逻辑（类似 insert）
        // 你的代码：
        for (int level = currentLevel; level >= 0; level--) {
            while (current->forward[level] != nullptr && 
                   current->forward[level]->key < key) {
                current = current->forward[level];
            }
            update[level] = current;
        }

        current = current->forward[0];

        // 如果找到目标节点
        if (current != nullptr && current->key == key) {
            // TODO: 实现删除节点的逻辑
            // 提示：
            // 1. 更新各层的指针（跳过要删除的节点）
            // 2. 删除节点
            // 3. 更新 currentLevel（如果最高层变空）
            // 4. 减少 size
            
            // 你的代码：
            for(int level = 0; level <= currentLevel; level++) {
                if (update[level]->forward[level] != current) {
                    break;
                }
                update[level]->forward[level] = current->forward[level];
            }
            delete current;
            while (currentLevel > 0 && header->forward[currentLevel] == nullptr) {
                currentLevel--;
            }
            size--;
            return true;
        }

        return false;
    }

    /**
     * 获取元素数量
     */
    int getSize() const {
        return size;
    }

    /**
     * 显示跳表结构（用于调试）
     * 
     * 输出示例：
     * Level 2: 1 -> 5 -> 9 -> NULL
     * Level 1: 1 -> 3 -> 5 -> 7 -> 9 -> NULL
     * Level 0: 1 -> 2 -> 3 -> 5 -> 6 -> 7 -> 8 -> 9 -> NULL
     */
    void display() const {
        std::cout << "\n===== Skip List Structure =====" << std::endl;
        std::cout << "Size: " << size << ", Current Level: " << currentLevel << std::endl;
        
        for (int level = currentLevel; level >= 0; level--) {
            std::cout << "Level " << level << ": ";
            SkipListNode<K, V>* node = header->forward[level];
            while (node != nullptr) {
                std::cout << node->key << ":" << node->value << " -> ";
                node = node->forward[level];
            }
            std::cout << "NULL" << std::endl;
        }
        std::cout << "==============================\n" << std::endl;
    }

    /**
     * 检查跳表的正确性（用于测试）
     * 验证：
     * 1. 每层都是有序的
     * 2. 指针的一致性
     */
    bool validate() const {
        // TODO: 可选实现，用于验证跳表结构的正确性
        
        return true;
    }
};

}  // namespace raft_kv

#endif  // RAFT_KV_STORE_SKIPLIST_H
