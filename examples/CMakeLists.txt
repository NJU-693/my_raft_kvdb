# 示例程序

# 原有示例程序（待实现）
# add_executable(single_node single_node.cpp)
# target_link_libraries(single_node storage util)

# add_executable(cluster cluster.cpp)
# target_link_libraries(cluster raft network storage util)

# nextIndex 更新机制演示
add_executable(nextindex_update_demo nextindex_update_demo.cpp)
target_link_libraries(nextindex_update_demo raft storage util)

# 日志冲突解决演示
add_executable(conflict_resolution_demo conflict_resolution_demo.cpp)
target_link_libraries(conflict_resolution_demo raft storage util)
