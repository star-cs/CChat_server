# 设置服务器名称及输出目录
set(SERVER_NAME GateServer)
set(SERVER_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/bin/servers/${SERVER_NAME})
set(TEST_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/bin/tests/${SERVER_NAME})

# 查找源文件
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.cc")

# 创建主可执行文件（包含所有源文件）
add_executable(${SERVER_NAME}
    main.cc
    ${SRC_FILES}
)

# 链接到可执行文件
target_include_directories(${SERVER_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}/../_proto  # proto 生成头文件路径
)

# 链接公共库
target_link_libraries(${SERVER_NAME}
    PRIVATE proto_lib
            common_lib
)

force_redefine_file_macro_for_sources(${SERVER_NAME})

# 设置可执行程序输出目录
set_target_properties(${SERVER_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${SERVER_OUTPUT_DIR}
)

# 添加测试目录
add_subdirectory(tests)
