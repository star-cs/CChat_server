file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.cc" "src/*.hpp")

add_library(common_lib ${SRC_FILES})

set_target_properties(proto_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(common_lib PUBLIC
    Boost::system
    Boost::filesystem
    pthread
    jsoncpp
    hiredis
    mysqlcppconn
    spdlog::spdlog_header_only
)

target_include_directories(common_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

force_redefine_file_macro_for_sources(common_lib)