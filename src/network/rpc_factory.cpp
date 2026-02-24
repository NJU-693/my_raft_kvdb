#include "network/rpc_server.h"
#include "network/rpc_client.h"
#include "network/grpc_rpc_server.h"
#include "network/grpc_rpc_client.h"

namespace network {

std::unique_ptr<RaftRPCServer> createRPCServer(raft::RaftNode* node) {
    return std::make_unique<GrpcRPCServer>(node);
}

std::unique_ptr<RaftRPCClient> createRPCClient(const std::string& address) {
    return std::make_unique<GrpcRPCClient>(address);
}

} // namespace network
