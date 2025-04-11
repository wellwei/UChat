//
// Created by wellwei on 2025/4/2.
//

#ifndef GATESERVER_GLOBAL_H
#define GATESERVER_GLOBAL_H

#include <nlohmann/json.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <iostream>
#include <chrono>
#include <utility>
#include <cassert>
#include <grpcpp/grpcpp.h>
#include <queue>
#include <sw/redis++/redis++.h>

enum ErrorCode {
    SUCCESS = 0,
    JSON_PARSE_ERROR = 1001,
    RPC_ERROR = 1002,
    INVALID_PARAMETER = 1003,
    TOO_MANY_REQUESTS = 1004,
    VERIFY_CODE_ERROR = 1005,
    EMAIL_EXISTS = 1006,
    USERNAME_EXISTS = 1007,
    MYSQL_ERROR = 1008,
};

enum RequestType {
    GET = 1,
    POST = 2,
    PUT = 3,
};

#endif //GATESERVER_GLOBAL_H
