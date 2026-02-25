# 编译指南

本文档详细说明如何在不同平台上编译 Raft KV 分布式存储系统。

## 目录

- [系统要求](#系统要求)
- [依赖安装](#依赖安装)
- [编译步骤](#编译步骤)
- [编译选项](#编译选项)
- [故障排除](#故障排除)

## 系统要求

### 必需

- C++17 或更高版本的编译器
- CMake 3.14 或更高版本
- gRPC 1.22.0 或更高版本
- Protocol Buffers 3.0 或更高版本
- pthread 库

### 可选

- Google Test（用于运行单元测试）
- Doxygen（用于生成文档）

## 依赖安装

### Ubuntu / Debian

```bash
# 更新包列表
sudo apt-get update

# 安装编译工具
sudo apt-get install -y build-essential cmake

# 安装 gRPC 和 Protocol Buffers
sudo apt-get install -y \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    pkg-config

# 安装 Google Test（可选）
sudo apt-get install -y libgtest-dev
```

### CentOS / RHEL

```bash
# 安装编译工具
sudo yum groupinstall "Development Tools"
sudo yum install cmake3

# 安装 gRPC 和 Protocol Buffers
# 注意：可能需要从源码编译或使用第三方仓库
sudo yum install grpc-devel protobuf-devel
```

### macOS

```bash
# 使用 Homebrew 安装
brew install cmake grpc protobuf

# 安装 Google Test（可选）
brew install googletest
```

### Windows

推荐使用 WSL (Windows Subsystem for Linux) 并按照 Ubuntu 的步骤安装。

或者使用 vcpkg：

```powershell
# 安装 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 安装依赖
.\vcpkg install grpc protobuf gtest
```

## 编译步骤

### 基本编译

```bash
# 1. 进入项目目录
cd /path/to/raft-kv-store

# 2. 创建构建目录
mkdir -p build
cd build

# 3. 配置 CMake
cmake ..

# 4. 编译
make -j$(nproc)
```

编译成功后，可执行文件位于 `build/bin/` 目录：

```
build/bin/
├── raft_node_server      # Raft 节点服务器
└── kv_client_cli         # KV 客户端命令行工具
```

### 编译测试

```bash
# 配置时启用测试
cmake .. -DBUILD_TESTS=ON

# 编译
make -j$(nproc)

# 运行所有测试
ctest

# 或运行特定测试
./bin/skiplist_test
./bin/kv_store_test
./bin/raft_node_test
```

### 编译示例程序

```bash
# 配置时启用示例
cmake .. -DBUILD_EXAMPLES=ON

# 编译
make -j$(nproc)

# 运行示例
./bin/rpc_demo
./bin/nextindex_update_demo
./bin/conflict_resolution_demo
```

### 完整编译（包含测试和示例）

```bash
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
make -j$(nproc)
```

## 编译选项

### CMake 选项

| 选项 | 默认值 | 说明 |
|-----|--------|------|
| `BUILD_TESTS` | ON | 是否编译测试 |
| `BUILD_EXAMPLES` | OFF | 是否编译示例程序 |
| `CMAKE_BUILD_TYPE` | Release | 构建类型（Debug/Release/RelWithDebInfo/MinSizeRel） |

### 使用示例

```bash
# Debug 模式编译（包含调试符号）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 模式编译（优化性能）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 指定编译器
cmake .. -DCMAKE_CXX_COMPILER=g++-10

# 指定安装路径
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
```

## 编译目标

### 主要目标

```bash
# 编译所有目标
make

# 只编译节点服务器
make raft_node_server

# 只编译客户端工具
make kv_client_cli

# 编译所有测试
make all_tests

# 编译特定测试
make skiplist_test
make kv_store_test
make raft_node_test
```

### 清理

```bash
# 清理编译产物
make clean

# 完全清理（删除 build 目录）
cd ..
rm -rf build
```

## 安装

```bash
# 编译后安装到系统
sudo make install

# 卸载
sudo make uninstall
```

默认安装路径：
- 可执行文件：`/usr/local/bin/`
- 库文件：`/usr/local/lib/`
- 头文件：`/usr/local/include/`

## 故障排除

### 问题 1：找不到 gRPC

**错误信息**：
```
CMake Error: Could not find gRPC
```

**解决方法**：

1. 确认 gRPC 已安装：
```bash
pkg-config --modversion grpc++
```

2. 如果未安装，按照[依赖安装](#依赖安装)部分安装

3. 如果已安装但 CMake 找不到，设置 PKG_CONFIG_PATH：
```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
cmake ..
```

### 问题 2：Protocol Buffers 版本不匹配

**错误信息**：
```
protobuf version mismatch
```

**解决方法**：

确保 protoc 和 libprotobuf 版本一致：

```bash
protoc --version
pkg-config --modversion protobuf
```

如果不一致，重新安装：

```bash
sudo apt-get remove libprotobuf-dev protobuf-compiler
sudo apt-get install libprotobuf-dev protobuf-compiler
```

### 问题 3：编译器不支持 C++17

**错误信息**：
```
error: 'std::optional' has not been declared
```

**解决方法**：

使用支持 C++17 的编译器：

```bash
# 安装较新版本的 GCC
sudo apt-get install g++-9

# 指定编译器
cmake .. -DCMAKE_CXX_COMPILER=g++-9
```

### 问题 4：链接错误

**错误信息**：
```
undefined reference to `grpc::...'
```

**解决方法**：

1. 确认 gRPC 库已正确安装
2. 清理并重新编译：
```bash
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 问题 5：内存不足

**错误信息**：
```
c++: fatal error: Killed signal terminated program cc1plus
```

**解决方法**：

减少并行编译任务数：

```bash
# 使用单线程编译
make

# 或使用较少的线程
make -j2
```

### 问题 6：权限错误

**错误信息**：
```
Permission denied
```

**解决方法**：

确保对项目目录有写权限：

```bash
# 检查权限
ls -la

# 修改权限（如果需要）
chmod -R u+w .
```

## 交叉编译

### ARM 平台

```bash
# 安装交叉编译工具链
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# 配置 CMake
cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=arm \
    -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
    -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++

# 编译
make -j$(nproc)
```

## 性能优化编译

### 启用 LTO（链接时优化）

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

make -j$(nproc)
```

### 启用本地 CPU 优化

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-march=native -mtune=native"

make -j$(nproc)
```

## 验证编译

### 运行测试

```bash
# 运行所有测试
ctest --output-on-failure

# 运行特定测试
ctest -R skiplist_test -V
```

### 检查可执行文件

```bash
# 查看依赖库
ldd build/bin/raft_node_server

# 查看符号表
nm build/bin/raft_node_server | grep main

# 检查文件类型
file build/bin/raft_node_server
```

## 持续集成

### GitHub Actions 示例

```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc
    
    - name: Configure
      run: cmake -B build -DBUILD_TESTS=ON
    
    - name: Build
      run: cmake --build build -j$(nproc)
    
    - name: Test
      run: cd build && ctest --output-on-failure
```

## 获取帮助

如果遇到编译问题：

1. 查看 CMake 输出的详细错误信息
2. 确认所有依赖已正确安装
3. 尝试清理并重新编译
4. 查看项目 Issues 或提交新 Issue

## 相关文档

- [快速开始指南](QUICKSTART.md)
- [部署指南](docs/deployment-guide.md)
- [开发文档](docs/architecture.md)
