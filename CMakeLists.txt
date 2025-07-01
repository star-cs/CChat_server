cmake_minimum_required(VERSION 3.15)
project(MyServerProject LANGUAGES CXX)

include (_cmake/utils.cmake)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-Wno-builtin-macro-redefined)

# 启用测试支持
enable_testing()
# 生成编译数据库文件
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置全局输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/_lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/_lib)

# 引入系统头文件目录 (jsoncpp)
include_directories(/usr/include/jsoncpp)

include_directories(./Common/src)

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

# 查找 proto 文件
file(GLOB PROTO_FILES "_proto/*.proto")

# 创建 proto 静态库 target
add_library(proto_lib ${PROTO_FILES})
# 启用位置无关代码
set_target_properties(proto_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 链接必要库
target_link_libraries(proto_lib PUBLIC
    protobuf::libprotobuf
    gRPC::grpc
    gRPC::grpc++
)

# 头文件目录
target_include_directories(proto_lib PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/_proto
)

# 获取 grpc_cpp_plugin 位置
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)

# 生成 .pb.cc / .pb.h 文件
protobuf_generate(
    TARGET proto_lib
    LANGUAGE cpp
)

# 生成 .grpc.pb.cc / .grpc.pb.h 文件
protobuf_generate(
    TARGET proto_lib
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}"
)


add_subdirectory(Common)

# 添加子目录
add_subdirectory(GateServer)
add_subdirectory(StatusServer)
add_subdirectory(ChatServer)

# 写CMake还是ChatGpt好用
