cmake_minimum_required(VERSION 3.16)
project(cpp_chat_cluster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 输出目录统一放到 build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 常用警告（学习阶段非常推荐）
add_compile_options(-Wall -Wextra -Wpedantic)

# =============================================
# 告诉编译器：muduo 的头文件在哪
# =============================================
# 这样你代码里写 #include <muduo/net/TcpServer.h>
# 编译器就会去这个目录下找 muduo/net/TcpServer.h
include_directories(/root/build/release-install-cpp11/include)

# =============================================
# 告诉链接器：muduo 的 .a 库文件在哪个目录
# =============================================
# 光知道头文件不够，链接时要找到 libmuduo_net.a 等文件
# link_directories 就是告诉链接器："去这个文件夹里找库"
link_directories(/root/build/release-install-cpp11/lib)

# =============================================
# server 可执行文件
# =============================================
file(GLOB SOURCES "src/server/*.cpp")
add_executable(chatserver ${SOURCES})

# target_link_libraries = 告诉链接器：chatserver 这个程序需要用到哪些库
# 链接顺序有讲究：依赖别人的放前面，被依赖的放后面
#   muduo_net 依赖 muduo_base → net 放前面
#   muduo 底层用了 pthread（多线程）→ pthread 放最后
target_link_libraries(chatserver
    muduo_net
    muduo_base
    pthread
)

# =============================================
# client 可执行文件
# =============================================
file(GLOB SOURCES "src/client/*.cpp")
add_executable(chatclient ${SOURCES})

target_link_libraries(chatclient
    muduo_net
    muduo_base
    pthread
)
