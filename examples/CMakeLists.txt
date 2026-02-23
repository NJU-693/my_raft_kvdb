# 示例程序

add_executable(single_node single_node.cpp)
target_link_libraries(single_node storage util)

add_executable(cluster cluster.cpp)
target_link_libraries(cluster raft network storage util)
