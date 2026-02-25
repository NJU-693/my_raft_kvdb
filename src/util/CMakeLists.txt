# Util 库
add_library(util
    config.cpp
)

target_include_directories(util PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
