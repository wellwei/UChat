//
// Created by wellwei on 2025/4/1.
//

#ifndef GATESERVER_LOGICSYSTEM_H
#define GATESERVER_LOGICSYSTEM_H

#include "Singleton.h"
#include "global.h"

class HttpConnection;

/**
 * HTTP 请求路由类
 */
class LogicSystem : public Singleton<LogicSystem>,
                    public std::enable_shared_from_this<LogicSystem> {
    friend class Singleton<LogicSystem>;

public:
    using HttpRequestHandler = std::function<void(std::shared_ptr<HttpConnection>)>;
    using HandlerMap = std::unordered_map<std::string, HttpRequestHandler>;

    ~LogicSystem() = default;

    bool handleGetRequest(const std::string &url, std::shared_ptr<HttpConnection> connection);

    bool handlePostRequest(const std::string &url, std::shared_ptr<HttpConnection> connection);

    bool handlePutRequest(const std::string &url, std::shared_ptr<HttpConnection> connection);

    void registerHandler(RequestType type, const std::string &url, const HttpRequestHandler &handler);

private:

    LogicSystem();

    HandlerMap get_handlers_;
    HandlerMap post_handlers_;
    HandlerMap put_handlers_;
};


#endif //GATESERVER_LOGICSYSTEM_H
