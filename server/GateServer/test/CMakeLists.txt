file(GLOB TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

find_package(redis++ CONFIG REQUIRED)

add_executable(tests ${TEST_FILES}
        TestRedisMgr.h
        TestIsValid.h
        TestMysql.h
        TestLogger.h)

target_link_libraries(tests
        GateServerLib
        redis++::redis++_static
)

# 将项目根目录下的 config.ini 文件复制到测试目录
configure_file(
        ${CMAKE_SOURCE_DIR}/config.ini
        ${CMAKE_CURRENT_BINARY_DIR}/config.ini
        COPYONLY
)