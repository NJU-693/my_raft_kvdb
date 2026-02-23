# 跳表实现教程 - 动手练习

## 学习目标

通过完成跳表的 `insert()` 和 `remove()` 函数，深入理解跳表的工作原理。

## 前置知识

### 1. 跳表的层级结构

```
Level 2: 1 -----------------> 9
Level 1: 1 -----> 5 --------> 9
Level 0: 1 -> 3 -> 5 -> 7 -> 9  (完整链表)
```

- 第 0 层包含所有元素（有序链表）
- 上层是下层的"快速通道"
- 查找时从高层开始，逐层下降

### 2. 关键概念

**update 数组**：
- 记录每一层中，新节点应该插入位置的前驱节点
- 用于更新指针，将新节点链接到跳表中

**随机层数**：
- 新节点的层数是随机生成的
- 概率为 0.5 时，期望层数为 log₂(n)
- 保证跳表的平衡性

## 任务一：实现 insert() 函数

### 步骤 1：查找插入位置

你需要从最高层开始，找到每一层中新节点应该插入的位置。

```cpp
// 在 insert() 函数中，找到这段 TODO 注释
// TODO: 实现查找插入位置的逻辑

// 提示代码框架：
for (int level = currentLevel; level >= 0; level--) {
    // 在当前层向右移动，直到：
    // 1. 到达层的末尾（forward[level] == nullptr）
    // 2. 或者下一个节点的 key >= 目标 key
    while (current->forward[level] != nullptr && 
           current->forward[level]->key < key) {
        current = current->forward[level];
    }
    
    // 记录当前层的前驱节点
    update[level] = current;
}
```

**理解要点**：
- 为什么从高层开始？因为高层跳过的元素多，查找更快
- 为什么要记录 update？因为插入时需要更新这些节点的指针

### 步骤 2：插入新节点

找到位置后，需要创建新节点并更新指针。

```cpp
// TODO: 实现插入新节点的逻辑

// 1. 生成随机层数
int newLevel = randomLevel();

// 2. 如果新层数超过当前最高层
if (newLevel > currentLevel) {
    // 需要更新 update 数组中新增层的前驱节点（都是 header）
    for (int level = currentLevel + 1; level <= newLevel; level++) {
        update[level] = header;
    }
    currentLevel = newLevel;  // 更新当前最高层
}

// 3. 创建新节点
SkipListNode<K, V>* newNode = new SkipListNode<K, V>(key, value, newLevel);

// 4. 更新各层的指针
for (int level = 0; level <= newLevel; level++) {
    // 新节点的 forward[level] 指向原来的下一个节点
    newNode->forward[level] = update[level]->forward[level];
    
    // 前驱节点的 forward[level] 指向新节点
    update[level]->forward[level] = newNode;
}
```

**理解要点**：
- 为什么要随机层数？保证跳表的平衡性，避免退化成链表
- 指针更新顺序很重要：先设置新节点的指针，再更新前驱节点的指针

### 插入过程图解

假设要插入 key=4：

```
初始状态：
Level 1: 1 -----> 5 --------> 9
Level 0: 1 -> 3 -> 5 -> 7 -> 9

查找后 update 数组：
update[1] = node(1)  // Level 1 中，4 应该在 1 之后
update[0] = node(3)  // Level 0 中，4 应该在 3 之后

假设随机层数为 1，插入后：
Level 1: 1 -> 4 -> 5 --------> 9
Level 0: 1 -> 3 -> 4 -> 5 -> 7 -> 9
```

## 任务二：实现 remove() 函数

### 步骤 1：查找目标节点

与 insert 类似，需要找到每一层中目标节点的前驱。

```cpp
// TODO: 实现查找目标节点的逻辑

for (int level = currentLevel; level >= 0; level--) {
    while (current->forward[level] != nullptr && 
           current->forward[level]->key < key) {
        current = current->forward[level];
    }
    update[level] = current;
}

// 移动到可能的目标节点
current = current->forward[0];
```

### 步骤 2：删除节点

如果找到目标节点，需要更新指针并释放内存。

```cpp
// 如果找到目标节点
if (current != nullptr && current->key == key) {
    // TODO: 实现删除节点的逻辑
    
    // 1. 更新各层的指针（跳过要删除的节点）
    for (int level = 0; level <= currentLevel; level++) {
        // 如果当前层不包含要删除的节点，停止
        if (update[level]->forward[level] != current) {
            break;
        }
        
        // 跳过要删除的节点
        update[level]->forward[level] = current->forward[level];
    }
    
    // 2. 释放节点内存
    delete current;
    
    // 3. 更新 currentLevel（如果最高层变空）
    while (currentLevel > 0 && header->forward[currentLevel] == nullptr) {
        currentLevel--;
    }
    
    // 4. 减少元素数量
    size--;
    
    return true;
}
```

**理解要点**：
- 为什么要检查 `update[level]->forward[level] != current`？
  因为高层可能不包含要删除的节点
- 为什么要更新 currentLevel？
  如果删除的是最高层的唯一节点，需要降低最高层

### 删除过程图解

假设要删除 key=5：

```
初始状态：
Level 1: 1 -----> 5 --------> 9
Level 0: 1 -> 3 -> 5 -> 7 -> 9

查找后 update 数组：
update[1] = node(1)  // Level 1 中，5 的前驱是 1
update[0] = node(3)  // Level 0 中，5 的前驱是 3

删除后：
Level 1: 1 -----------------> 9
Level 0: 1 -> 3 -> 7 -> 9
```

## 测试你的实现

### 编译和运行

```bash
# 在项目根目录
mkdir build && cd build
cmake ..
cmake --build .

# 运行测试
./bin/skiplist_test
```

### 预期输出

如果实现正确，你应该看到：
```
========================================
    Skip List Unit Tests
========================================

Test 1: Basic Insert and Search
✓ Basic insert and search passed

===== Skip List Structure =====
Size: 3, Current Level: X
Level X: 1:one -> 3:three -> NULL
Level 0: 1:one -> 2:two -> 3:three -> NULL
==============================

...

========================================
  All tests passed! ✓
========================================
```

## 调试技巧

### 1. 使用 display() 函数

在关键位置调用 `display()` 查看跳表结构：

```cpp
list.insert(1, "one");
list.display();  // 查看插入后的结构

list.remove(1);
list.display();  // 查看删除后的结构
```

### 2. 添加调试输出

在你的实现中添加 cout 语句：

```cpp
std::cout << "Inserting key: " << key << std::endl;
std::cout << "Random level: " << newLevel << std::endl;
```

### 3. 使用断言

验证关键条件：

```cpp
assert(newLevel >= 0 && newLevel < MAX_LEVEL);
assert(update[0] != nullptr);
```

## 常见错误

### 1. 指针更新顺序错误

❌ 错误：
```cpp
update[level]->forward[level] = newNode;
newNode->forward[level] = update[level]->forward[level];  // 已经被修改了！
```

✅ 正确：
```cpp
newNode->forward[level] = update[level]->forward[level];
update[level]->forward[level] = newNode;
```

### 2. 忘记更新 currentLevel

插入时如果新层数超过当前最高层，必须更新 currentLevel。

### 3. 内存泄漏

删除节点后必须 `delete current`。

### 4. update 数组未初始化

在新增层时，必须将 update 数组中新层的值设为 header。

## 扩展练习

完成基本实现后，可以尝试：

1. **实现范围查询**：
   ```cpp
   std::vector<std::pair<K, V>> range(const K& start, const K& end);
   ```

2. **实现迭代器**：
   支持 `for (auto& [key, value] : list)` 语法

3. **性能测试**：
   比较跳表和 std::map 的性能

4. **并发优化**：
   使用细粒度锁或无锁算法

## 参考资料

- 原始论文：[Skip Lists: A Probabilistic Alternative to Balanced Trees](https://15721.courses.cs.cmu.edu/spring2018/papers/08-oltpindexes1/pugh-skiplists-cacm1990.pdf)
- Redis 实现：[t_zset.c](https://github.com/redis/redis/blob/unstable/src/t_zset.c)
- LevelDB 实现：[skiplist.h](https://github.com/google/leveldb/blob/main/db/skiplist.h)

## 完成检查清单

- [ ] 理解跳表的层级结构
- [ ] 理解 update 数组的作用
- [ ] 完成 insert() 函数的查找部分
- [ ] 完成 insert() 函数的插入部分
- [ ] 完成 remove() 函数
- [ ] 所有测试通过
- [ ] 使用 display() 验证结构正确
- [ ] 理解时间复杂度为什么是 O(log n)

祝你学习愉快！🚀
