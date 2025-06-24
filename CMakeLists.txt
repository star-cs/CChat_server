cmake_minimum_required(VERSION 3.15)
project(MyServerProject LANGUAGES CXX)

include (_cmake/utils.cmake)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wno-builtin-macro-redefined)

# 启用测试支持
enable_testing()
# 生成编译数据库文件
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置全局输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 引入系统头文件目录 (jsoncpp)
include_directories(/usr/include/jsoncpp)

# 如果 gRPC 安装在 /usr/local/lib/cmake，加入 CMake 查找路径
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake")

# 查找第三方库
# find_package(Boost 1.88 REQUIRED COMPONENTS system thread filesystem)
find_package(Boost REQUIRED COMPONENTS system filesystem)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(spdlog REQUIRED)
# find_package(hiredis REQUIRED)    # hiredis没有cmake，不需要find_package
# find_package(MySQLConnector REQUIRED) # 查找 MySQL Connector/C++

# 添加子目录
add_subdirectory(GateServer)
add_subdirectory(StatusServer)
add_subdirectory(ChatServer)

# 写CMake还是ChatGpt好用
