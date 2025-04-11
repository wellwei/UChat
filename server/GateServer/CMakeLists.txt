cmake_minimum_required(VERSION 3.28)

project(GateServer)

set(CMAKE_CXX_STANDARD 20)

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 13.6)
endif ()

cmake_policy(SET CMP0167 NEW)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS beast asio filesystem)
find_package(Protobuf CONFIG REQUIRED)
find_package(grpc CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)

set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/generated")
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

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
        $<BUILD_INTERFACE:${GENERATED_DIR}>
)

add_library(GateServerLib SHARED ${SOURCES}
        src/CServer.cpp
        src/CServer.h
        src/HttpConnection.cpp
        src/HttpConnection.h
        src/Singleton.h
        src/LogicSystem.cpp
        src/LogicSystem.h
        src/util.h
        src/global.h
        src/VerifyGrpcClient.cpp
        src/VerifyGrpcClient.h
        src/SectionInfo.h
        src/ConfigMgr.cpp
        src/ConfigMgr.h
        src/global.cpp
        src/ServicePool.cpp
        src/ServicePool.h
        src/GrpcStubPool.cpp
        src/GrpcStubPool.h
        src/RedisConnPool.cpp
        src/RedisConnPool.h
        src/RedisMgr.cpp
        src/RedisMgr.h
        src/RequestHandlers.h
        src/MysqlConnPool.cpp
        src/MysqlConnPool.h
        src/MysqlDao.cpp
        src/MysqlDao.h
        src/MysqlMgr.cpp
        src/MysqlMgr.h)

target_include_directories(GateServerLib PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

# 添加 MySQL Connector/C++
set(MYSQL_CPPCON /usr/local/mysql/connector-c++-8.3.0)
target_include_directories(GateServerLib PUBLIC ${MYSQL_CPPCON}/include)
target_link_directories(GateServerLib PUBLIC ${MYSQL_CPPCON}/lib64)

target_link_libraries(GateServerLib PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(GateServerLib PUBLIC Boost::asio Boost::beast Boost::filesystem)
target_link_libraries(GateServerLib PUBLIC proto_lib)
target_link_libraries(GateServerLib PUBLIC redis++::redis++_static)
target_link_libraries(GateServerLib PUBLIC mysqlcppconn)

add_executable(GateServer main.cpp)
target_link_libraries(GateServer PRIVATE GateServerLib)

add_subdirectory(test)
enable_testing()

# 拷贝配置文件到构建目录
configure_file(
        ${CMAKE_SOURCE_DIR}/config.ini
        ${CMAKE_BINARY_DIR}/config.ini
        COPYONLY
)
