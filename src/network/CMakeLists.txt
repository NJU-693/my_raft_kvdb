# 网络模块源文件
set(NETWORK_SOURCES
    simple_rpc_server.cpp
    simple_rpc_client.cpp
    rpc_factory.cpp
)

add_library(network ${NETWORK_SOURCES})
target_include_directories(network PUBLIC ${PROJECT_SOURCE_DIR}/include)

# 链接 raft 库（用于访问 RaftNode）
target_link_libraries(network PUBLIC raft)
