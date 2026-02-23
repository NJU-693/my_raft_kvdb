# 网络模块源文件
set(NETWORK_SOURCES
    rpc_service.cpp
    rpc_client.cpp
)

add_library(network ${NETWORK_SOURCES})
target_include_directories(network PUBLIC ${PROJECT_SOURCE_DIR}/include)
