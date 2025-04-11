//
// Created by wellwei on 2025/4/1.
//

#include "LogicSystem.h"
#include "global.h"
#include "HttpConnection.h"
#include "RequestHandlers.h"

LogicSystem::LogicSystem() {
    registerHandler(RequestType::GET, "/get_test", RequestHandlerFuncs::handle_get_test);

    registerHandler(RequestType::POST, "/get_verifycode", RequestHandlerFuncs::handle_post_getverifycode);

    registerHandler(RequestType::POST, "/register", RequestHandlerFuncs::handle_post_register);
}

void
LogicSystem::registerHandler(RequestType type, const std::string &url, const LogicSystem::HttpRequestHandler &handler) {
    switch (type) {
        case GET:
            get_handlers_[url] = handler;
            break;
        case POST:
            post_handlers_[url] = handler;
            break;
        case PUT:
            break;
        default:
            std::cerr << "Unsupported request type" << std::endl;
            break;
    }
}

bool LogicSystem::handleGetRequest(const std::string &url, std::shared_ptr<HttpConnection> connection) {
    auto it = get_handlers_.find(url);
    if (it != get_handlers_.end()) {
        try {
            it->second(std::move(connection));
            return true;
        }
        catch (const std::exception &e) {
            std::cerr << "Exception in GET handler: " << e.what() << std::endl;
        }
    }
    return false;
}

bool LogicSystem::handlePostRequest(const std::string &url, std::shared_ptr<HttpConnection> connection) {
    auto it = post_handlers_.find(url);
    if (it != post_handlers_.end()) {
        try {
            it->second(std::move(connection));
            return true;
        }
        catch (const std::exception &e) {
            std::cerr << "Exception in POST handler: " << e.what() << std::endl;
        }
    }
    return false;
}