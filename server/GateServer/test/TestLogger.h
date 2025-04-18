//
// Created by wellwei on 2025/4/15.
//

#ifndef GATESERVER_TESTLOGGER_H
#define GATESERVER_TESTLOGGER_H

#include <iostream>
#include "Logger.h"

void TestLogger() {
    std::cout << "----------------------------------" << std::endl;
    std::cout << "Test Logger" << std::endl;
    std::cout << "----------------------------------" << std::endl;

    LOG_CRITICAL("{} Critical message", "This is a critical message");
    LOG_DEBUG("{} Debug message", "This is a debug message");
    LOG_INFO("{} Info message", "This is an info message");
    LOG_WARN("{} Warning message", "This is a warning message");
    LOG_ERROR("{} Error message", "This is an error message");

}

#endif //GATESERVER_TESTLOGGER_H
