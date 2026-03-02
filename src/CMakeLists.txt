# 添加子模块
add_subdirectory(util)
add_subdirectory(storage)
add_subdirectory(raft)
add_subdirectory(network)
add_subdirectory(client)

# 构建 Raft 节点服务器可执行文件
add_executable(raft_node_server raft_node_server.cpp)
target_link_libraries(raft_node_server
    raft
    storage
    network
    util
    raft_proto
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    pthread
)

# 构建 KV 客户端命令行工具
add_executable(kv_client_cli kv_client_cli.cpp)
target_link_libraries(kv_client_cli
    client
    util
    raft_proto
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    pthread
)