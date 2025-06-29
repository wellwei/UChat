//
// Created by wellwei on 2025/4/1.
//

#include "LogicSystem.h"
#include "global.h"
#include "HttpConnection.h"
#include "RequestHandlers.h"
#include "Logger.h"

LogicSystem::LogicSystem() {
    registerHandler(RequestType::GET, "/get_test", RequestHandlerFuncs::handle_get_test);
    registerHandler(RequestType::POST, "/get_verifycode", RequestHandlerFuncs::handle_post_getverifycode);
    registerHandler(RequestType::POST, "/register", RequestHandlerFuncs::handle_post_register);
    registerHandler(RequestType::POST, "/login", RequestHandlerFuncs::handle_post_login);
    registerHandler(RequestType::POST, "/reset_password", RequestHandlerFuncs::handle_post_resetPassword);
    registerHandler(RequestType::POST, "/get_chat_server", RequestHandlerFuncs::handle_post_getChatServer);
    registerHandler(RequestType::POST, "/get_user_profile", RequestHandlerFuncs::handle_post_getUserProfile);
    registerHandler(RequestType::POST, "/get_contacts", RequestHandlerFuncs::handle_post_getContacts);
    registerHandler(RequestType::POST, "/search_user", RequestHandlerFuncs::handle_post_searchUser);
    registerHandler(RequestType::POST, "/send_contact_request", RequestHandlerFuncs::handle_post_sendContactRequest);
    registerHandler(RequestType::POST, "/handle_contact_request", RequestHandlerFuncs::handle_post_handleContactRequest);
    registerHandler(RequestType::POST, "/get_contact_requests", RequestHandlerFuncs::handle_post_getContactRequests);
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
            put_handlers_[url] = handler;
            break;
        default:
            LOG_ERROR("Unknown request type: {}", static_cast<int>(type));
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
            LOG_ERROR("Exception in handlerRequest: {}", e.what());
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
            LOG_ERROR("Exception in handlerRequest: {}", e.what());
        }
    }
    return false;
}

bool LogicSystem::handlePutRequest(const std::string &url, std::shared_ptr<HttpConnection> connection) {
    auto it = put_handlers_.find(url);
    if (it != put_handlers_.end()) {
        try {
            it->second(std::move(connection));
            return true;
        }
        catch (const std::exception &e) {
            LOG_ERROR("Exception in handlerRequest: {}", e.what());
        }
    }
    return false;
}