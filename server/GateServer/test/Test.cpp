//
// Created by wellwei on 2025/4/8.
//

#include <iostream>

#include "TestRedisMgr.h"
#include "TestIsValid.h"
#include "TestMysql.h"
#include "TestLogger.h"

int main() {
    Logger::init("logs", "test");
    Logger::setLogLevel(spdlog::level::level_enum::debug);
//    TestRedis();
//    std::cout << "----------------------------------" << std::endl;
//    test_redispp();
//    TestRedisMgr();
//    std::cout << "----------------------------------" << std::endl;
//    TestIsValid();
    std::cout << "----------------------------------" << std::endl;
    TestMysql();
    std::cout << "----------------------------------" << std::endl;
    TestLogger();
    return 0;
}