# Raft 演示程序使用指南

本目录包含两个演示程序，用于帮助理解 Raft 算法的核心机制。

## 演示程序列表

### 1. nextindex_update_demo
**功能**：演示 Raft 中 `nextIndex` 的动态更新机制

**展示内容**：
- 场景1：正常的日志复制流程（nextIndex 如何通过响应处理自动更新）
- 场景2：日志冲突解决（nextIndex 如何在失败时回退并重试）

**学习重点**：理解 `nextIndex` 不需要手动更新，而是通过 `handleAppendEntriesResponse()` 自动维护

### 2. conflict_resolution_demo
**功能**：演示 Raft 日志冲突解决的多轮迭代过程

**展示内容**：
- 场景1：简单的多轮回退（展示迭代过程）
- 场景2：最坏情况（需要回退到起点）

**学习重点**：理解冲突解决是迭代的，如果一次回退后仍不匹配，会继续回退直到成功

## 编译步骤

### 方法1：在 build 目录中编译（推荐）

```bash
# 1. 进入 build 目录
cd build

# 2. 配置 CMake（开启示例程序构建）
cmake .. -DBUILD_EXAMPLES=ON

# 3. 编译两个演示程序
make nextindex_update_demo conflict_resolution_demo

# 或者编译所有示例程序
make
```

### 方法2：从项目根目录编译

```bash
# 1. 配置并构建
mkdir -p build
cd build
cmake .. -DBUILD_EXAMPLES=ON
make nextindex_update_demo conflict_resolution_demo
```

### 编译选项说明

- `-DBUILD_EXAMPLES=ON`：开启示例程序构建（默认为 OFF）
- `-DBUILD_TESTS=ON`：开启测试构建（默认为 ON）

## 运行步骤

编译成功后，可执行文件位于 `build/bin/` 目录：

### 运行 nextindex_update_demo

```bash
# 在 build 目录中
./bin/nextindex_update_demo

# 或从项目根目录
./build/bin/nextindex_update_demo
```

**预期输出**：
- 详细的步骤说明
- Leader 和 Follower 的状态变化
- nextIndex 的更新过程
- 可视化的 RPC 参数

### 运行 conflict_resolution_demo

```bash
# 在 build 目录中
./bin/conflict_resolution_demo

# 或从项目根目录
./build/bin/conflict_resolution_demo
```

**预期输出**：
- 多轮 AppendEntries 尝试的完整过程
- 每一轮的冲突信息和回退逻辑
- 最终同步成功的结果

## 示例输出截图

### nextindex_update_demo 输出示例

```
════════════════════════════════════════════════════════════
 核心结论：nextIndex 通过响应处理动态更新
════════════════════════════════════════════════════════════

1. 成功时：nextIndex[i] = lastLogIndex + 1
   → 下次从最新位置继续

2. 失败时：nextIndex[i] = conflictIndex (回退)
   → 下次从更早位置重试

3. 追加日志：不更新 nextIndex
   → 下次 RPC 自动包含新日志

4. 形成反馈循环：发送 → 响应 → 更新 → 发送...
   → 自适应地追上 Leader 的进度
```

### conflict_resolution_demo 输出示例

```
╔══════════════════════════════════════════════════════════════════════╗
║                          核心结论                                    ║
╚══════════════════════════════════════════════════════════════════════╝

1. 冲突解决是迭代的，不是一次完成的
   ❌ 错误理解：一次回退就要找到正确位置
   ✅ 正确理解：多次尝试，逐步回退，直到成功

2. 每次失败都提供新的冲突信息
3. 收敛性保证
4. 优化效果
5. 这是正常的，不是 bug！
```

## 一键运行脚本

为了方便，可以创建一个脚本一次性编译并运行所有演示：

```bash
#!/bin/bash
# run_demos.sh

echo "编译演示程序..."
cd build
cmake .. -DBUILD_EXAMPLES=ON
make nextindex_update_demo conflict_resolution_demo

echo ""
echo "=========================================="
echo "运行演示1：nextIndex 更新机制"
echo "=========================================="
./bin/nextindex_update_demo

echo ""
echo ""
echo "=========================================="
echo "运行演示2：日志冲突解决"
echo "=========================================="
./bin/conflict_resolution_demo

echo ""
echo "所有演示运行完成！"
```

使用方法：
```bash
chmod +x run_demos.sh
./run_demos.sh
```

## 故障排除

### 问题1：找不到可执行文件

**错误**：`bash: ./bin/nextindex_update_demo: No such file or directory`

**解决**：
1. 确保已经编译成功
2. 检查是否在 build 目录中运行
3. 检查 BUILD_EXAMPLES 选项是否开启

### 问题2：编译错误

**错误**：`CMake Error: Cannot find source file`

**解决**：
1. 确保源文件存在于 `examples/` 目录
2. 重新运行 `cmake ..`
3. 清理 build 目录：`rm -rf build && mkdir build`

### 问题3：链接错误

**错误**：`undefined reference to ...`

**解决**：
1. 确保 raft 和 storage 库已编译
2. 运行 `make` 编译所有目标
3. 检查 CMakeLists.txt 中的链接配置

## 相关文档

- [nextIndex 更新机制详解](../docs/nextIndex-update-mechanism.md)
- [日志冲突解决详解](../docs/conflict-resolution-detailed.md)
- [Raft 架构文档](../docs/architecture.md)
- [Raft 算法说明](../docs/raft-algorithm.md)

## 贡献

如果你想添加新的演示程序：

1. 在 `examples/` 目录创建新的 `.cpp` 文件
2. 在 `examples/CMakeLists.txt` 中添加编译规则：
   ```cmake
   add_executable(your_demo your_demo.cpp)
   target_link_libraries(your_demo raft storage util)
   ```
3. 更新本 README 文档

## 许可证

与主项目相同
