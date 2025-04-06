cmake_minimum_required(VERSION 3.28)

project(GateServer)

set(CMAKE_CXX_STANDARD 20)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS beast asio filesystem)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

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
        $<INSTALL_INTERFACE:include>
)

add_executable(GateServer ${SOURCES}
        src/main.cpp
        src/CServer.cpp
        src/CServer.h
        src/HttpConnection.cpp
        src/HttpConnection.h
        src/Singleton.h
        src/LogicSystem.cpp
        src/LogicSystem.h
        src/utility.h
        src/global.h
        src/CaptchaGrpcClient.cpp
        src/CaptchaGrpcClient.h
        src/SectionInfo.h
        src/ConfigMgr.cpp
        src/ConfigMgr.h
        src/global.cpp
        src/IOServicePool.cpp
        src/IOServicePool.h)

target_link_libraries(GateServer PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(GateServer PRIVATE Boost::asio Boost::beast Boost::filesystem)
target_link_libraries(GateServer PRIVATE proto_lib)

# 拷贝配置文件到构建目录
configure_file(
        ${CMAKE_SOURCE_DIR}/config.ini
        ${CMAKE_BINARY_DIR}/config.ini
        COPYONLY
)