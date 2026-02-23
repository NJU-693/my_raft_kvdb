# Raft 模块源文件
set(RAFT_SOURCES
    raft_node.cpp
    log_entry.cpp
)

add_library(raft ${RAFT_SOURCES})
target_include_directories(raft PUBLIC ${PROJECT_SOURCE_DIR}/include)
# Note: util module is optional (tasks 4.1, 4.2), not linking for now
# target_link_libraries(raft storage util)
target_link_libraries(raft storage)
