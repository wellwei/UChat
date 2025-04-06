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

#include "message.grpc.pb.h"
#include "message.pb.h"
#include "ConfigMgr.h"
#include "Singleton.h"

enum ErrorCode {
    SUCCESS = 0,
    JSON_PARSE_ERROR = 1001,
    RPC_ERROR = 1002,
};

enum RequestType {
    GET = 1,
    POST = 2,
    PUT = 3,
};

#endif //GATESERVER_GLOBAL_H
