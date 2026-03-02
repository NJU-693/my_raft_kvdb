cmake_minimum_required(VERSION 3.14)
project(RaftKVStore VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找 Protobuf
find_package(Protobuf REQUIRED)

# 查找 gRPC（使用 pkg-config）
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(GRPCPP REQUIRED grpc++>=1.22.0)

# 包含目录
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR})  # 添加项目根目录以支持 proto/raft.pb.h 路径
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${GRPC_INCLUDE_DIRS})

# 添加 proto 生成的源文件库
add_library(raft_proto
    proto/raft.pb.cc
    proto/raft.grpc.pb.cc
)
target_link_libraries(raft_proto PUBLIC
    ${PROTOBUF_LIBRARIES}
    ${GRPC_LIBRARIES}
)
target_include_directories(raft_proto PUBLIC 
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/proto
)

# 子目录
add_subdirectory(src)

# 可选：启用测试
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# 可选：构建示例
option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()