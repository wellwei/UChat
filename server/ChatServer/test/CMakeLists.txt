add_executable(chatserver_chat_api_integration_test
        ChatApiIntegrationTest.cpp
        ${CMAKE_SOURCE_DIR}/src/ConfigMgr.cpp
        ${CMAKE_SOURCE_DIR}/src/RedisConnPool.cpp
        ${CMAKE_SOURCE_DIR}/src/RedisMgr.cpp
        ${CMAKE_SOURCE_DIR}/src/ChatApiService.cpp
        ${CMAKE_SOURCE_DIR}/src/DeliverApiClient.cpp
        ${CMAKE_SOURCE_DIR}/src/UserServiceClient.cpp
        ${CMAKE_SOURCE_DIR}/src/ChannelPool.cpp)

target_link_libraries(chatserver_chat_api_integration_test PRIVATE
        proto_lib
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        jwt-cpp::jwt-cpp
        Boost::asio
        Boost::system
        Boost::uuid
        redis++::redis++_static)

target_include_directories(chatserver_chat_api_integration_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
        ${GENERATED_DIR})

add_executable(chatserver_send_qps_benchmark
        ChatApiQpsBenchmark.cpp
        ${CMAKE_SOURCE_DIR}/src/ConfigMgr.cpp
        ${CMAKE_SOURCE_DIR}/src/RedisConnPool.cpp
        ${CMAKE_SOURCE_DIR}/src/RedisMgr.cpp
        ${CMAKE_SOURCE_DIR}/src/ChatApiService.cpp
        ${CMAKE_SOURCE_DIR}/src/DeliverApiClient.cpp
        ${CMAKE_SOURCE_DIR}/src/UserServiceClient.cpp
        ${CMAKE_SOURCE_DIR}/src/ChannelPool.cpp)

target_link_libraries(chatserver_send_qps_benchmark PRIVATE
        proto_lib
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        jwt-cpp::jwt-cpp
        Boost::asio
        Boost::system
        Boost::uuid
        redis++::redis++_static)

target_include_directories(chatserver_send_qps_benchmark PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}
        ${GENERATED_DIR})

configure_file(
        ${CMAKE_SOURCE_DIR}/config.ini
        ${CMAKE_CURRENT_BINARY_DIR}/config.ini
        COPYONLY
)

add_test(
        NAME chatserver_chat_api_integration_test
        COMMAND chatserver_chat_api_integration_test
)

set_tests_properties(chatserver_chat_api_integration_test PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
