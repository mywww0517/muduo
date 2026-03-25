cmake_minimum_required(VERSION 3.16)
project(cpp_chat_cluster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 输出目录统一放到 build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 常用警告（学习阶段非常推荐）
add_compile_options(-Wall -Wextra -Wpedantic)

# 头文件搜索路径
include_directories(/root/build/release-install-cpp11/include)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty)
include_directories(${CMAKE_SOURCE_DIR}/src/db)
include_directories(${CMAKE_SOURCE_DIR}/src/model)
include_directories(${CMAKE_SOURCE_DIR}/src/server)
include_directories(${CMAKE_SOURCE_DIR}/src/common)  # json_helper.hpp 等公共头文件

# 库文件搜索路径
link_directories(/root/build/release-install-cpp11/lib)

# 公共依赖（避免重复写）
set(MUDUO_LIBS muduo_net muduo_base pthread)

# server 可执行文件
add_executable(chatserver
    src/server/chatserver.cpp
    src/server/chatservice.cpp
    src/db/db.cpp
    src/model/usermodel.cpp
    src/model/friendmodel.cpp
    src/model/groupmodel.cpp
    src/model/offlinemsgmodel.cpp
    src/common/crypto.cpp
)
target_link_libraries(chatserver ${MUDUO_LIBS} mysqlclient crypto)

add_executable(echoserver
    src/server/echoserver.cpp
)
target_link_libraries(echoserver ${MUDUO_LIBS})


# client 可执行文件
add_executable(chatclient
    src/client/chatclient.cpp
)
target_link_libraries(chatclient ${MUDUO_LIBS})

add_executable(echoclient
    src/client/echoclient.cpp
)
target_link_libraries(echoclient ${MUDUO_LIBS})

# 粘包压力测试
add_executable(stresstest src/client/stresstest.cpp)
target_link_libraries(stresstest ${MUDUO_LIBS})

add_executable(test_db
    src/db/test_db.cpp
    src/db/db.cpp
)
target_link_libraries(test_db
    muduo_base
    mysqlclient
    pthread
)

add_executable(test_model
    src/db/test_model.cpp
    src/db/db.cpp
    src/model/usermodel.cpp
)
target_link_libraries(test_model
    muduo_base
    mysqlclient
    pthread
)
