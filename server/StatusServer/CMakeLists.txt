cmake_minimum_required(VERSION 3.10)
project(StatusServer)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-osx)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost CONFIG COMPONENTS asio beast filesystem)
find_package(gRPC CONFIG REQUIRED)
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(StatusServer main.cpp)

target_link_libraries(StatusServer PUBLIC
        gRPC::grpc++
        Boost::asio Boost::beast Boost::filesystem
        unofficial::libmysql::libmysql unofficial::mysql-connector-cpp::connector-jdbc
        nlohmann_json::nlohmann_json
)
