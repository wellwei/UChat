cmake_minimum_required(VERSION 3.10)

project(UserServerStressTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(Triplet "x64-osx")
elseif (LINUX)
    set(Triplet "x64-linux")
endif()

set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/../vcpkg_installed/${Triplet})

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/../proto_generated")

add_executable(UserServerStressTest
    StressTest.cpp
    GrpcClient.cpp
)

# 获取 proto_lib 的目标属性
add_dependencies(UserServerStressTest proto_lib)

target_link_libraries(UserServerStressTest PRIVATE
    proto_lib
    protobuf::libprotobuf
    gRPC::grpc++
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

target_include_directories(UserServerStressTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GENERATED_DIR}
    ${CMAKE_SOURCE_DIR}/../src
)
