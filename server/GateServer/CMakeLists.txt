cmake_minimum_required(VERSION 3.10)
project(GateServer)

if(NOT DEFINED CMAKE_PREFIX_PATH OR "${CMAKE_PREFIX_PATH}" STREQUAL "")
    if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-osx")
        set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-osx")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/cmake-build-debug/vcpkg_installed/x64-osx")
        set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/cmake-build-debug/vcpkg_installed/x64-osx")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)

set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/protoc_generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

foreach (PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

    add_custom_command(
            OUTPUT
            ${GENERATED_DIR}/${PROTO_NAME}.pb.cc
            ${GENERATED_DIR}/${PROTO_NAME}.pb.h
            ${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.cc
            ${GENERATED_DIR}/${PROTO_NAME}.grpc.pb.h
            COMMAND $<TARGET_FILE:protobuf::protoc>
            ARGS
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
            --grpc_out=${GENERATED_DIR}
            --cpp_out=${GENERATED_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
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

# 为 proto_lib 添加 fPIC 编译选项
set_property(TARGET proto_lib PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(GateServerLib SHARED ${SOURCES}
        src/Singleton.h
        src/global.h
        src/VerifyGrpcClient.cpp
        src/VerifyGrpcClient.h
        src/SectionInfo.h
        src/ConfigMgr.cpp
        src/ConfigMgr.h
        src/global.cpp
        src/GrpcStubPool.h
        src/RedisConnPool.cpp
        src/RedisConnPool.h
        src/RedisMgr.cpp
        src/RedisMgr.h
        src/Logger.cpp
        src/Logger.h
        src/UserServiceClient.cpp
        src/UserServiceClient.h
        src/SessionRegistry.h
        src/SessionRegistry.cpp
        src/ChatSession.h
        src/ChatSession.cpp
        src/ChatTunnelService.h
        src/ChatTunnelService.cpp
        src/DeliverApiService.h
        src/DeliverApiService.cpp
        src/GatewayServer.h
        src/GatewayServer.cpp
        src/ChatApiClient.h
        src/ChatApiClient.cpp
        src/ChatControlApiService.h
        src/ChatControlApiService.cpp
        src/TimerWheel.h
)

target_include_directories(GateServerLib PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${GENERATED_DIR}>
)

target_link_libraries(GateServerLib PUBLIC
        nlohmann_json::nlohmann_json
        proto_lib
        redis++::redis++_static
        spdlog::spdlog
        jwt-cpp::jwt-cpp
)

# 主程序
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
