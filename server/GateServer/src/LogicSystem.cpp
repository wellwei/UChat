//
// Created by wellwei on 2025/4/1.
//

#include "LogicSystem.h"
#include "global.h"
#include "HttpConnection.h"
#include "CaptchaGrpcClient.h"

LogicSystem::LogicSystem() {
    registerHandler(RequestType::GET, "/get_test", [](const std::shared_ptr<HttpConnection> &connection) {
        boost::beast::ostream(connection->res_.body()) << "GET request received!";

        int i = 0;
        for (const auto &param: connection->get_params_) {
            beast::ostream(connection->res_.body()) << "\n" << i++ << ": " << param.first << " = " << param.second;
        }
        return true;
    });

    registerHandler(RequestType::POST, "/get_captcha", [](const std::shared_ptr<HttpConnection> &connection) {
        std::string body = boost::beast::buffers_to_string(connection->req_.body().data());

        nlohmann::json json_data;
        nlohmann::json response;

        connection->res_.set(http::field::content_type, "application/json");
        try {
            json_data = nlohmann::json::parse(body);
        }
        catch (const nlohmann::json::parse_error &e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            response["error"] = ErrorCode::JSON_PARSE_ERROR;
            boost::beast::ostream(connection->res_.body()) << response.dump();
            return true;
        }

        std::string email = json_data["email"].get<std::string>();

        message::CaptchaResponse captcha_response = CaptchaGrpcClient::getInstance()->getCaptcha(email);
        json_data["captcha"] = captcha_response.captcha();
        json_data["error"] = captcha_response.code();
        json_data["email"] = email;
        boost::beast::ostream(connection->res_.body()) << json_data.dump();
        return true;
    });
}

void LogicSystem::registerHandler(RequestType type, const std::string &url, LogicSystem::HttpRequestHandler handler) {
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