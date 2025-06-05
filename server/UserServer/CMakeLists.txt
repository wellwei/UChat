cmake_minimum_required(VERSION 3.10)

project(UserServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-osx)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)

set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/proto_generated")
file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/*.proto")

foreach (PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

    add_custom_command(
            OUTPUT
            ${GENERATED_DIR}/${PROTO_NAME}.pb.cc
            ${GENERATED_DIR}/${PROTO_NAME}.pb.h
            ${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.cc
            ${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.h
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            ARGS
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
            --grpc_out=${GENERATED_DIR}
            --cpp_out=${GENERATED_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            -I=${CMAKE_CURRENT_SOURCE_DIR}
            ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating gRPC code for ${PROTO_NAME}"
    )

    list(APPEND proto_lib
            ${GENERATED_DIR}/${PROTO_NAME}.pb.cc
            ${GENERATED_DIR}/${PROTO_NAME}.pb.h
            ${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.cc
            ${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.h
    )
endforeach ()

add_library(proto_lib OBJECT ${proto_lib})
target_link_libraries(proto_lib PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
        gRPC::grpc++_reflection
)
target_include_directories(proto_lib PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
)

add_executable(UserServer
        main.cpp
        src/MysqlMgr.cpp
        src/MysqlMgr.h
        src/util.h
        src/Logger.h
        src/Singleton.h
        src/MysqlConnPool.cpp
        src/MysqlConnPool.h
        src/UserServer.cpp
        src/UserServer.h
        src/ConfigMgr.h
        src/ConfigMgr.cpp
        src/Auth.h
        src/Auth.cpp
        src/RedisConnPool.cpp
        src/RedisConnPool.h
        src/RedisMgr.cpp
        src/RedisMgr.h
)

target_link_libraries(UserServer PUBLIC
        proto_lib
        gRPC::grpc++
        gRPC::grpc++_reflection
        unofficial::libmysql::libmysql
        unofficial::mysql-connector-cpp::connector-jdbc
        jwt-cpp::jwt-cpp
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        redis++::redis++_static
)

target_include_directories(UserServer PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
)


# 拷贝配置文件
configure_file(
        ${CMAKE_SOURCE_DIR}/config.ini
        ${CMAKE_BINARY_DIR}/config.ini
        COPYONLY
)