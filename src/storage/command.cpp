#include "storage/command.h"
#include <sstream>
#include <stdexcept>

namespace raft_kv {

Command::Command() : type_(CommandType::GET), key_(""), value_("") {}

Command::Command(CommandType type, const std::string& key, const std::string& value)
    : type_(type), key_(key), value_(value) {}

CommandType Command::getType() const {
    return type_;
}

const std::string& Command::getKey() const {
    return key_;
}

const std::string& Command::getValue() const {
    return value_;
}

std::string Command::serialize() const {
    std::ostringstream oss;
    
    switch (type_) {
        case CommandType::PUT:
            oss << "PUT:" << key_ << ":" << value_;
            break;
        case CommandType::GET:
            oss << "GET:" << key_;
            break;
        case CommandType::DELETE:
            oss << "DELETE:" << key_;
            break;
        default:
            throw std::runtime_error("Unknown command type");
    }
    
    return oss.str();
}

Command Command::deserialize(const std::string& str) {
    if (str.empty()) {
        throw std::invalid_argument("Cannot deserialize empty string");
    }
    
    // 查找第一个冒号
    size_t first_colon = str.find(':');
    if (first_colon == std::string::npos) {
        throw std::invalid_argument("Invalid command format: missing colon");
    }
    
    // 提取命令类型
    std::string type_str = str.substr(0, first_colon);
    CommandType type;
    
    if (type_str == "PUT") {
        type = CommandType::PUT;
    } else if (type_str == "GET") {
        type = CommandType::GET;
    } else if (type_str == "DELETE") {
        type = CommandType::DELETE;
    } else {
        throw std::invalid_argument("Invalid command type: " + type_str);
    }
    
    // 提取键和值
    std::string key;
    std::string value;
    
    if (type == CommandType::PUT) {
        // PUT 命令格式: PUT:key:value
        // 查找第二个冒号，它是 key 和 value 的分隔符
        // 这样 key 不能包含冒号，但 value 可以包含任意数量的冒号
        
        size_t second_colon = str.find(':', first_colon + 1);
        if (second_colon == std::string::npos) {
            // 只有一个冒号，缺少 value
            throw std::invalid_argument("Invalid PUT command format: missing value");
        }
        
        key = str.substr(first_colon + 1, second_colon - first_colon - 1);
        value = str.substr(second_colon + 1);
        
        if (key.empty()) {
            throw std::invalid_argument("Key cannot be empty");
        }
    } else {
        // GET 和 DELETE 命令格式: GET:key 或 DELETE:key
        // key 可以包含冒号，所以直接取第一个冒号后的所有内容
        key = str.substr(first_colon + 1);
        
        if (key.empty()) {
            throw std::invalid_argument("Key cannot be empty");
        }
    }
    
    return Command(type, key, value);
}

}  // namespace raft_kv
