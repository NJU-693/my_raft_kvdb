# 快速参考 - 演示程序编译运行

## 🚀 快速开始（3 步）

```bash
# 第1步：进入 build 目录
cd build

# 第2步：配置并编译
cmake .. -DBUILD_EXAMPLES=ON && make nextindex_update_demo conflict_resolution_demo

# 第3步：运行演示
./bin/nextindex_update_demo        # 演示1：nextIndex 更新机制
./bin/conflict_resolution_demo     # 演示2：日志冲突解决
```

## 📋 完整命令清单

### Windows (WSL) / Linux / macOS

#### 首次编译
```bash
cd /mnt/d/桂子清嘉/desktop/my_raft  # 进入项目根目录
cd build
cmake .. -DBUILD_EXAMPLES=ON
make nextindex_update_demo conflict_resolution_demo
```

#### 重新编译（修改代码后）
```bash
cd build
make nextindex_update_demo conflict_resolution_demo
```

#### 运行演示程序
```bash
# 在 build 目录中
./bin/nextindex_update_demo
./bin/conflict_resolution_demo

# 或从项目根目录
./build/bin/nextindex_update_demo
./build/bin/conflict_resolution_demo
```

## 🎯 演示程序说明

| 程序 | 功能 | 运行时间 |
|-----|------|---------|
| `nextindex_update_demo` | 展示 nextIndex 如何自动更新 | ~1秒 |
| `conflict_resolution_demo` | 展示多轮冲突解决过程 | ~1秒 |

## 💡 常见用法

### 只编译不运行
```bash
cd build
cmake .. -DBUILD_EXAMPLES=ON
make nextindex_update_demo
```

### 编译所有示例
```bash
cd build
cmake .. -DBUILD_EXAMPLES=ON
make  # 编译所有目标
```

### 清理重新编译
```bash
cd build
rm -rf *
cmake .. -DBUILD_EXAMPLES=ON
make nextindex_update_demo conflict_resolution_demo
```

### 查看可执行文件
```bash
ls -lh build/bin/
```

## 🔍 验证编译成功

编译成功后，应该看到：
```
[100%] Built target nextindex_update_demo
[100%] Built target conflict_resolution_demo
```

可执行文件位置：
```
build/bin/nextindex_update_demo
build/bin/conflict_resolution_demo
```

## ⚠️ 常见错误

### 错误1：找不到 build 目录
```bash
# 解决：创建 build 目录
mkdir build
cd build
cmake .. -DBUILD_EXAMPLES=ON
```

### 错误2：CMake 未找到源文件
```bash
# 解决：确保在正确的目录
pwd  # 应该显示 .../my_raft/build
ls ../examples/  # 应该能看到 .cpp 文件
```

### 错误3：可执行文件不存在
```bash
# 解决：检查编译是否成功
make nextindex_update_demo
# 检查文件是否生成
ls bin/nextindex_update_demo
```

## 📖 更多信息

详细使用指南：[examples/README.md](README.md)
