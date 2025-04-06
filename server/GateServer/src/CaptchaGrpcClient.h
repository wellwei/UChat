//
// Created by wellwei on 2025/4/4.
//

#ifndef GATESERVER_CAPTCHAGRPCCLIENT_H
#define GATESERVER_CAPTCHAGRPCCLIENT_H

#include "global.h"
#include "Singleton.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::CaptchaRequest;
using message::CaptchaResponse;
using message::CaptchaService;

class CaptchaGrpcClient : public Singleton<CaptchaGrpcClient> {
    friend class Singleton<CaptchaGrpcClient>;

public:
    CaptchaResponse getCaptcha(const std::string &email);

private:
    CaptchaGrpcClient();

    std::unique_ptr<CaptchaService::Stub> stub_; // gRPC 客户端存根
};


#endif //GATESERVER_CAPTCHAGRPCCLIENT_H
