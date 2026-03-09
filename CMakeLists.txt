cmake_minimum_required(VERSION 3.16)
project(cpp_chat_cluster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 输出目录统一放到 build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 常用警告（学习阶段非常推荐）
add_compile_options(-Wall -Wextra -Wpedantic)

# server
add_executable(chatserver
  src/server/main.cpp
)

# client
add_executable(chatclient
  src/client/main.cpp
)
