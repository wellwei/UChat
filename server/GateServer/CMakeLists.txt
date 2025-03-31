cmake_minimum_required(VERSION 3.28)
project(GateServer)

set(CMAKE_CXX_STANDARD 20)

find_package(nlohmann_json CONFIG REQUIRED)

add_executable(GateServer main.cpp)

target_link_libraries(GateServer PRIVATE nlohmann_json::nlohmann_json)