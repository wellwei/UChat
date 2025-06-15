cmake_minimum_required(VERSION 3.10)
project(StatusServer)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-osx)

set(CMAKE_CXX_STANDARD 20)

find_package(gRPC CONFIG REQUIRED)
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)    

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
        $<BUILD_INTERFACE:${GENERATED_DIR}>
)

add_executable(StatusServer 
        src/StatusServer.h
        src/StatusServer.cpp
        src/ConfigMgr.cpp
        src/ConfigMgr.h
        src/Singleton.h
        src/Logger.h
        src/util.h
        main.cpp)

target_link_libraries(StatusServer PUBLIC
        proto_lib
        unofficial::libmysql::libmysql unofficial::mysql-connector-cpp::connector-jdbc
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

target_include_directories(StatusServer PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
)

configure_file(
        ${CMAKE_SOURCE_DIR}/config.ini
        ${CMAKE_BINARY_DIR}/config.ini
        COPYONLY
)