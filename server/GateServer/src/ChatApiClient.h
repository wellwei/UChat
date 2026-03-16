#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include "im.grpc.pb.h"
#include "GrpcStubPool.h"
#include "Singleton.h"

// Client stub pool for calling ChatServer's ChatApi service.
// Single instance (singleton). Used by ChatSession to forward Send/Ack/Pull.
class ChatApiClient : public Singleton<ChatApiClient> {
    friend class Singleton<ChatApiClient>;

public:
    ~ChatApiClient() = default;

    im::SendMessageResp SendMessage(const im::SendMessageReq& req);
    im::AckUpResp Ack(const im::AckUpReq& req);

    // Sync: pull missed messages based on client's MaxSeqID (replaces GetOfflineMessages)
    im::SyncUpResp Sync(const im::SyncUpReq& req);

private:
    ChatApiClient();

    std::unique_ptr<GrpcStubPool<im::ChatApi>> stub_pool_;
};
