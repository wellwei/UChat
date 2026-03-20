//
// Created by goodnight on 26-3-18.
//

#ifndef CHANNELPOOL_H
#define CHANNELPOOL_H
#include "grpcpp/channel.h"


class ChannelPool {
public:
    ChannelPool() = default;
    ~ChannelPool() = default;

    std::shared_ptr<grpc::Channel> GetChannel(const std::string& name);

private:

    std::map<std::string, std::shared_ptr<grpc::Channel>> channels_map_;
    std::mutex channel_mu_;
    std::mutex create_mu_;
};



#endif //CHANNELPOOL_H
