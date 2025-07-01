# 设置测试输出目录
file(MAKE_DIRECTORY ${TEST_OUTPUT_DIR})

# 获取所有测试源文件
file(GLOB TEST_SOURCES "test_*.cc")

# 遍历每个测试源文件
foreach(TEST_SOURCE ${TEST_SOURCES})
    # 从文件名提取测试名称（不含路径、不含扩展名）
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    # 创建测试可执行程序
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    force_redefine_file_macro_for_sources(${TEST_NAME})

    # 设置可执行文件输出路径
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR}
    )

    # 链接 GateServerLib 和它依赖的库（GateServerLib 已经连上 GateServerProto）
    target_link_libraries(${TEST_NAME} PRIVATE
        proto_lib
        common_lib
    )
endforeach()
