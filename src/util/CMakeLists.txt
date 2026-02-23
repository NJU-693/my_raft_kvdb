# 工具模块源文件
set(UTIL_SOURCES
    logger.cpp
    config.cpp
)

add_library(util ${UTIL_SOURCES})
target_include_directories(util PUBLIC ${PROJECT_SOURCE_DIR}/include)
