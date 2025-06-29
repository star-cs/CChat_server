# 设置服务器名称及输出目录
set(SERVER_NAME GateServer)
set(SERVER_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/bin/servers/${SERVER_NAME})
set(TEST_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/bin/tests/${SERVER_NAME})

# 查找源文件
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h" "src/*.cc" "src/*.hpp")

# 创建主静态库
add_library(${SERVER_NAME}Lib SHARED
    ${SRC_FILES}
)

target_link_libraries(${SERVER_NAME}Lib PUBLIC
    proto_lib
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
    ${CMAKE_CURRENT_BINARY_DIR}/../_proto             # 添加 cmake build proto 生成头文件
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
# add_subdirectory(tests)
