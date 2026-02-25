# Raft 模块源文件
set(RAFT_SOURCES
    raft_node.cpp
    log_entry.cpp
)

add_library(raft ${RAFT_SOURCES})
target_include_directories(raft PUBLIC ${PROJECT_SOURCE_DIR}/include)
# Link network library for RPC client functionality
target_link_libraries(raft storage network)
