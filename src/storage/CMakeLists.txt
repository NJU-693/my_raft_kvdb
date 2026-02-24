# 存储模块源文件
set(STORAGE_SOURCES
    command.cpp
    kv_store.cpp
    persister.cpp
)

add_library(storage ${STORAGE_SOURCES})
target_include_directories(storage PUBLIC ${PROJECT_SOURCE_DIR}/include)
