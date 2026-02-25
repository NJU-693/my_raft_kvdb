# 存储模块
add_subdirectory(storage)

# 网络模块（必须在 raft 之前，因为 raft 依赖 network）
add_subdirectory(network)

# Raft 模块
add_subdirectory(raft)

# 客户端模块
add_subdirectory(client)

# 工具模块
add_subdirectory(util)

# Raft 节点服务器可执行文件
add_executable(raft_node_server raft_node_server.cpp)
target_link_libraries(raft_node_server
    raft
    storage
    network
    util
    pthread
)
target_include_directories(raft_node_server PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# KV 客户端命令行工具
add_executable(kv_client_cli kv_client_cli.cpp)
target_link_libraries(kv_client_cli
    client
    network
    util
    pthread
)
target_include_directories(kv_client_cli PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
