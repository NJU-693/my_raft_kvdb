#include "storage/kv_store.h"
#include "storage/command.h"
#include <stdexcept>

namespace raft_kv {

KVStore::KVStore() {
    // 跳表会在其构造函数中自动初始化
}

KVStore::~KVStore() {
    // 跳表会在其析构函数中自动清理所有节点
}

bool KVStore::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mtx_);
    return store_.insert(key, value);
}

bool KVStore::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(mtx_);
    return store_.search(key, value);
}

bool KVStore::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx_);
    return store_.remove(key);
}

void KVStore::applyCommand(const std::string& command) {
    // 反序列化命令
    Command cmd = Command::deserialize(command);
    
    // 根据命令类型执行相应操作
    switch (cmd.getType()) {
        case CommandType::PUT: {
            std::lock_guard<std::mutex> lock(mtx_);
            store_.insert(cmd.getKey(), cmd.getValue());
            break;
        }
        case CommandType::GET: {
            // GET 命令通常不需要修改状态，但为了接口一致性保留
            // 在实际的 Raft 系统中，GET 可能不会通过日志复制
            std::string value;
            std::lock_guard<std::mutex> lock(mtx_);
            store_.search(cmd.getKey(), value);
            break;
        }
        case CommandType::DELETE: {
            std::lock_guard<std::mutex> lock(mtx_);
            store_.remove(cmd.getKey());
            break;
        }
        default:
            throw std::invalid_argument("Unknown command type");
    }
}

int KVStore::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return store_.getSize();
}

std::string KVStore::applyCommandWithResult(const std::string& command) {
    // 反序列化命令
    Command cmd = Command::deserialize(command);
    
    // 根据命令类型执行相应操作
    switch (cmd.getType()) {
        case CommandType::PUT: {
            std::lock_guard<std::mutex> lock(mtx_);
            store_.insert(cmd.getKey(), cmd.getValue());
            return "OK";
        }
        case CommandType::GET: {
            std::string value;
            std::lock_guard<std::mutex> lock(mtx_);
            bool found = store_.search(cmd.getKey(), value);
            if (found) {
                return "VALUE:" + value;
            } else {
                return "NOT_FOUND";
            }
        }
        case CommandType::DELETE: {
            std::lock_guard<std::mutex> lock(mtx_);
            bool removed = store_.remove(cmd.getKey());
            return removed ? "OK" : "NOT_FOUND";
        }
        default:
            throw std::invalid_argument("Unknown command type");
    }
}

}  // namespace raft_kv
