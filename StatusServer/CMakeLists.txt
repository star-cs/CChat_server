# 设置服务器名称及输出目录
set(SERVER_NAME StatusServer)
set(SERVER_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/bin/servers/${SERVER_NAME})
set(TEST_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/bin/tests/${SERVER_NAME})

# 查找 proto 文件
file(GLOB PROTO_FILES "proto/*.proto")

# 创建 proto 静态库 target
add_library(${SERVER_NAME}Proto ${PROTO_FILES})
# 启用位置无关代码
set_target_properties(${SERVER_NAME}Proto PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 链接必要库
target_link_libraries(${SERVER_NAME}Proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc
    gRPC::grpc++
)

# 头文件目录
target_include_directories(${SERVER_NAME}Proto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/proto
)

# 获取 grpc_cpp_plugin 位置
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)

# 生成 .pb.cc / .pb.h 文件
protobuf_generate(
    TARGET ${SERVER_NAME}Proto
    LANGUAGE cpp
)

# 生成 .grpc.pb.cc / .grpc.pb.h 文件
protobuf_generate(
    TARGET ${SERVER_NAME}Proto
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}"
)


# 查找源文件
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h" "src/*.cc" "src/*.hpp")

# 创建主静态库
add_library(${SERVER_NAME}Lib SHARED
    ${SRC_FILES}
)

target_link_libraries(${SERVER_NAME}Lib PUBLIC
    ${SERVER_NAME}Proto
    Boost::system
    Boost::filesystem
    pthread
    jsoncpp
    hiredis
    mysqlcppconn
    spdlog::spdlog_header_only
)

# 添加头文件路径
target_include_directories(${SERVER_NAME}Lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/proto             # 添加 cmake build proto 生成头文件
)

# 创建主可执行文件
add_executable(${SERVER_NAME} main.cc)

force_redefine_file_macro_for_sources(${SERVER_NAME}Lib)
force_redefine_file_macro_for_sources(${SERVER_NAME})

# 链接静态库到可执行文件
target_link_libraries(${SERVER_NAME} PRIVATE ${SERVER_NAME}Lib)

# 设置输出路径
set_target_properties(${SERVER_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${SERVER_OUTPUT_DIR}
)

# 添加测试目录
add_subdirectory(tests)
