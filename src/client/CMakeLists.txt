# 客户端库源文件
set(CLIENT_SOURCES
    kv_client.cpp
)

# 创建客户端库
add_library(client ${CLIENT_SOURCES})

# 包含目录
target_include_directories(client PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/proto
)

# 链接依赖库
target_link_libraries(client
    raft_proto
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
)
