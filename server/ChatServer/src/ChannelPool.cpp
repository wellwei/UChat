//
// Created by goodnight on 26-3-18.
//

#include "ChannelPool.h"

#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"

std::shared_ptr<grpc::Channel> ChannelPool::GetChannel(const std::string &name) {

    {
        std::lock_guard<std::mutex> get_lock(channel_mu_);
        if (channels_map_.count(name) ) {
            return channels_map_[name];
        }
    }

    std::lock_guard<std::mutex> create_lock(create_mu_);

    {
        std::lock_guard<std::mutex> get_lock(channel_mu_);
        if (channels_map_.count(name)) {
            return channels_map_[name];
        }
    }

    auto channel = grpc::CreateChannel(name, grpc::InsecureChannelCredentials());

    {
        std::lock_guard<std::mutex> get_lock(channel_mu_);
        channels_map_[name] = channel;
    }

    return channel;
}