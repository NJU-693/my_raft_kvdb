# 测试配置
# 这里可以使用 Google Test 或其他测试框架
# 示例使用简单的可执行文件

# 跳表节点测试
add_executable(skiplist_node_test skiplist_node_test.cpp)

# 跳表测试
add_executable(skiplist_test skiplist_test.cpp)

# 命令测试
add_executable(command_test command_test.cpp)
target_link_libraries(command_test storage)

# KV 存储测试
add_executable(kv_store_test kv_store_test.cpp)
target_link_libraries(kv_store_test storage)

# 日志条目测试
add_executable(log_entry_test log_entry_test.cpp)

# RPC 消息测试
add_executable(rpc_messages_test rpc_messages_test.cpp)

# Raft 节点测试
add_executable(raft_node_test raft_node_test.cpp)
target_link_libraries(raft_node_test raft storage)

# 领导者选举集成测试
add_executable(leader_election_integration_test leader_election_integration_test.cpp)
target_link_libraries(leader_election_integration_test raft storage)

# 日志复制测试
add_executable(log_replication_test log_replication_test.cpp)
target_link_libraries(log_replication_test raft storage)

# 日志复制集成测试
add_executable(log_replication_integration_test log_replication_integration_test.cpp)
target_link_libraries(log_replication_integration_test raft storage)

# 持久化测试
add_executable(persister_test persister_test.cpp)
target_link_libraries(persister_test storage)

# 添加测试
add_test(NAME SkipListNodeTest COMMAND skiplist_node_test)
add_test(NAME SkipListTest COMMAND skiplist_test)
add_test(NAME CommandTest COMMAND command_test)
add_test(NAME KVStoreTest COMMAND kv_store_test)
add_test(NAME LogEntryTest COMMAND log_entry_test)
add_test(NAME RPCMessagesTest COMMAND rpc_messages_test)
add_test(NAME RaftNodeTest COMMAND raft_node_test)
add_test(NAME LeaderElectionIntegrationTest COMMAND leader_election_integration_test)
add_test(NAME LogReplicationTest COMMAND log_replication_test)
add_test(NAME LogReplicationIntegrationTest COMMAND log_replication_integration_test)
add_test(NAME PersisterTest COMMAND persister_test)
