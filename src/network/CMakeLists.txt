# 网络模块源文件
set(NETWORK_SOURCES
    simple_rpc_server.cpp
    simple_rpc_client.cpp
    grpc_rpc_server.cpp
    grpc_rpc_client.cpp
    rpc_factory.cpp
)

add_library(network ${NETWORK_SOURCES})
target_include_directories(network PUBLIC 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}
)

# 链接 raft 库和 proto 库
target_link_libraries(network PUBLIC 
    raft
    raft_proto
)
