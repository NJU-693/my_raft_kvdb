# 跳表 (Skip List) 实现说明

## 1. 什么是跳表

跳表是一种基于链表的数据结构，通过在链表上建立多层索引来实现快速查找。它是平衡树的一种替代方案，实现简单但性能相当。

### 1.1 基本思想

普通链表查找需要 O(n) 时间，跳表通过建立多层索引，将查找时间降低到 O(log n)。

```
层级 3:  1 ────────────────────────────► 9
层级 2:  1 ─────────► 5 ───────────────► 9
层级 1:  1 ──► 3 ──► 5 ──► 7 ──────────► 9
层级 0:  1 ──► 3 ──► 5 ──► 7 ──► 8 ──► 9  (原始链表)
```

### 1.2 核心特性

- **时间复杂度**：
  - 查找：O(log n)
  - 插入：O(log n)
  - 删除：O(log n)

- **空间复杂度**：O(n)

- **优点**：
  - 实现简单，易于理解
  - 不需要复杂的旋转操作（相比红黑树）
  - 支持范围查询
  - 并发友好

- **缺点**：
  - 空间开销略大
  - 性能依赖随机数生成

## 2. 数据结构设计

### 2.1 节点结构

```cpp
template<typename K, typename V>
struct SkipListNode {
    K key;                              // 键
    V value;                            // 值
    std::vector<SkipListNode*> forward; // 各层的前向指针
    
    SkipListNode(K k, V v, int level) 
        : key(k), value(v), forward(level + 1, nullptr) {}
};
```

**说明**：
- `key`：节点的键，用于排序和查找
- `value`：节点存储的值
- `forward`：一个指针数组，`forward[i]` 指向第 i 层的下一个节点

### 2.2 跳表类

```cpp
template<typename K, typename V>
class SkipList {
private:
    static constexpr int MAX_LEVEL = 16;      // 最大层数
    static constexpr float PROBABILITY = 0.5;  // 晋升概率
    
    SkipListNode<K, V>* header;  // 头节点（哨兵节点）
    int currentLevel;             // 当前最高层数
    int size;                     // 元素数量
    std::mt19937 rng;            // 随机数生成器
    
    int randomLevel();            // 生成随机层数
    
public:
    SkipList();
    ~SkipList();
    
    bool insert(const K& key, const V& value);
    bool search(const K& key, V& value);
    bool remove(const K& key);
    int getSize() const { return size; }
    void display() const;
};
```

## 3. 核心算法

### 3.1 查找操作

**算法思路**：
1. 从最高层的头节点开始
2. 在当前层向右移动，直到下一个节点的 key 大于等于目标 key
3. 下降到下一层
4. 重复步骤 2-3，直到最底层
5. 检查找到的节点是否匹配

**伪代码**：
```
search(key):
    current = header
    for level from currentLevel down to 0:
        while current.forward[level] != null and current.forward[level].key < key:
            current = current.forward[level]
    
    current = current.forward[0]
    if current != null and current.key == key:
        return current.value
    else:
        return not found
```

**时间复杂度**：O(log n)

### 3.2 插入操作

**算法思路**：
1. 查找插入位置，记录每层的前驱节点
2. 随机生成新节点的层数
3. 创建新节点
4. 更新各层的指针

**伪代码**：
```
insert(key, value):
    update[MAX_LEVEL]  // 记录每层的前驱节点
    current = header
    
    // 查找插入位置
    for level from currentLevel down to 0:
        while current.forward[level] != null and current.forward[level].key < key:
            current = current.forward[level]
        update[level] = current
    
    // 检查是否已存在
    current = current.forward[0]
    if current != null and current.key == key:
        current.value = value  // 更新值
        return
    
    // 生成随机层数
    newLevel = randomLevel()
    if newLevel > currentLevel:
        for level from currentLevel + 1 to newLevel:
            update[level] = header
        currentLevel = newLevel
    
    // 创建新节点
    newNode = new SkipListNode(key, value, newLevel)
    
    // 更新指针
    for level from 0 to newLevel:
        newNode.forward[level] = update[level].forward[level]
        update[level].forward[level] = newNode
    
    size++
```

**时间复杂度**：O(log n)

### 3.3 删除操作

**算法思路**：
1. 查找目标节点，记录每层的前驱节点
2. 如果找到，更新各层的指针
3. 删除节点
4. 更新当前最高层数

**伪代码**：
```
remove(key):
    update[MAX_LEVEL]
    current = header
    
    // 查找目标节点
    for level from currentLevel down to 0:
        while current.forward[level] != null and current.forward[level].key < key:
            current = current.forward[level]
        update[level] = current
    
    current = current.forward[0]
    
    // 如果找到目标节点
    if current != null and current.key == key:
        // 更新指针
        for level from 0 to currentLevel:
            if update[level].forward[level] != current:
                break
            update[level].forward[level] = current.forward[level]
        
        delete current
        
        // 更新当前最高层数
        while currentLevel > 0 and header.forward[currentLevel] == null:
            currentLevel--
        
        size--
        return true
    
    return false
```

**时间复杂度**：O(log n)

### 3.4 随机层数生成

**算法思路**：
- 使用概率 p（通常为 0.5）决定是否晋升到更高层
- 层数从 0 开始，每次以概率 p 增加一层
- 最大不超过 MAX_LEVEL

**代码**：
```cpp
int randomLevel() {
    int level = 0;
    std::uniform_real_distribution<> dis(0.0, 1.0);
    while (dis(rng) < PROBABILITY && level < MAX_LEVEL - 1) {
        level++;
    }
    return level;
}
```

**期望层数**：log₂(n)

## 4. 实现细节

### 4.1 头节点（哨兵节点）

- 头节点不存储实际数据
- 简化边界条件处理
- 所有层的起点

### 4.2 层数选择

- **MAX_LEVEL = 16**：对于 2^16 = 65536 个元素足够
- **PROBABILITY = 0.5**：平衡空间和时间
- 可根据实际数据量调整

### 4.3 内存管理

- 析构函数需要释放所有节点
- 从最底层遍历删除所有节点

```cpp
~SkipList() {
    SkipListNode<K, V>* current = header->forward[0];
    while (current != nullptr) {
        SkipListNode<K, V>* next = current->forward[0];
        delete current;
        current = next;
    }
    delete header;
}
```

### 4.4 线程安全

- 基本实现不是线程安全的
- 需要在外部加锁保护
- 或使用细粒度锁优化并发性能

## 5. 性能分析

### 5.1 时间复杂度分析

**查找路径长度**：
- 期望路径长度：O(log n)
- 最坏情况：O(n)（概率极低）

**空间复杂度**：
- 期望空间：O(n)
- 每个节点平均层数：1/(1-p) = 2（当 p=0.5）

### 5.2 与其他数据结构对比

| 数据结构 | 查找 | 插入 | 删除 | 实现复杂度 |
|---------|------|------|------|-----------|
| 跳表    | O(log n) | O(log n) | O(log n) | 简单 |
| 红黑树  | O(log n) | O(log n) | O(log n) | 复杂 |
| AVL树   | O(log n) | O(log n) | O(log n) | 较复杂 |
| 哈希表  | O(1)* | O(1)* | O(1)* | 中等 |

*哈希表不支持有序遍历

### 5.3 实际性能

- 跳表的常数因子较小
- 缓存友好性不如数组
- 适合并发场景（如 Redis 的 Sorted Set）

## 6. 应用场景

### 6.1 KV 存储

- 支持快速的 Put/Get/Delete
- 支持范围查询
- 本项目的应用场景

### 6.2 数据库索引

- LevelDB、RocksDB 使用跳表作为 MemTable
- 支持有序遍历

### 6.3 并发数据结构

- Java ConcurrentSkipListMap
- 无锁或细粒度锁实现

## 7. 优化方向

### 7.1 范围查询

```cpp
std::vector<std::pair<K, V>> range(const K& start, const K& end) {
    std::vector<std::pair<K, V>> result;
    SkipListNode<K, V>* current = findFirstGreaterOrEqual(start);
    while (current != nullptr && current->key <= end) {
        result.push_back({current->key, current->value});
        current = current->forward[0];
    }
    return result;
}
```

### 7.2 并发优化

- 使用细粒度锁
- 无锁算法（CAS）
- 读写锁分离

### 7.3 内存优化

- 使用内存池
- 减少动态分配
- 压缩指针

## 8. 调试技巧

### 8.1 可视化

```cpp
void display() const {
    for (int level = currentLevel; level >= 0; level--) {
        std::cout << "Level " << level << ": ";
        SkipListNode<K, V>* node = header->forward[level];
        while (node != nullptr) {
            std::cout << node->key << ":" << node->value << " -> ";
            node = node->forward[level];
        }
        std::cout << "NULL" << std::endl;
    }
}
```

### 8.2 验证正确性

- 检查每层的有序性
- 检查指针的一致性
- 使用单元测试

## 9. 学习资源

- [Skip Lists: A Probabilistic Alternative to Balanced Trees](https://15721.courses.cs.cmu.edu/spring2018/papers/08-oltpindexes1/pugh-skiplists-cacm1990.pdf) - 原始论文
- [Redis Sorted Set 实现](https://github.com/redis/redis/blob/unstable/src/t_zset.c)
- [LevelDB SkipList 实现](https://github.com/google/leveldb/blob/main/db/skiplist.h)

## 10. 练习建议

1. 先实现基本的插入、查找、删除
2. 添加详细的注释和日志
3. 编写单元测试验证正确性
4. 实现可视化函数帮助理解
5. 尝试实现范围查询
6. 性能测试和优化
