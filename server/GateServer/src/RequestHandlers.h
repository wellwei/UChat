//
// Created by wellwei on 2025/4/9.
//

#ifndef GATESERVER_REQUESTHANDLERS_H
#define GATESERVER_REQUESTHANDLERS_H

#include "global.h"
#include "HttpConnection.h"
#include "util.h"
#include "RedisMgr.h"
#include "VerifyGrpcClient.h"
#include "MysqlMgr.h"


inline namespace RequestHandlerFuncs {

    inline void handle_get_test(const std::shared_ptr<HttpConnection> &connection) {
        boost::beast::ostream(connection->getRequest().body()) << "GET request received!";

        int i = 0;
        for (const auto &param: connection->getParams()) {
            beast::ostream(connection->getResponse().body()) << "\n" << i++ << ": " << param.first << " = "
                                                             << param.second;
        }
    }

    inline void handle_post_getverifycode(const std::shared_ptr<HttpConnection> &connection) {
        std::string body = boost::beast::buffers_to_string(connection->getRequest().body().data());

        nlohmann::json json_data;
        nlohmann::json response;

        connection->getResponse().set(http::field::content_type, "application/json");
        try {
            json_data = nlohmann::json::parse(body);
        }
        catch (const nlohmann::json::parse_error &e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            response["error"] = ErrorCode::JSON_PARSE_ERROR;
            response["message"] = "Invalid JSON format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        if (!json_data.contains("email") || !json_data["email"].is_string()) {
            std::cerr << "Invalid JSON format: missing or invalid 'email'" << std::endl;
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid parameter: 'email' is required and must be a string";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string email = json_data["email"].get<std::string>();

        if (!isValidEmail(email)) {
            std::cerr << "Invalid email format: " << email << std::endl;
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid email format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string throttle_key = "throttle_verify_" + email;
        auto redis_mgr = RedisMgr::getInstance();
        // 如果 Redis 中存在该键值，则表示请求过于频繁
        if (redis_mgr->exists(throttle_key)) {
            response["error"] = ErrorCode::TOO_MANY_REQUESTS;
            response["message"] = "Too many requests. Please try again later.";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        // 调用 gRPC 客户端获取验证码
        message::VerifyResponse Verify_response = VerifyGrpcClient::getInstance()->getVerifyCode(email);
        response["error"] = Verify_response.code();
        response["email"] = email;

        // 设置 Redis 键值的过期时间为 60 秒
        redis_mgr->set(throttle_key, "1", 60);

#ifdef DEBUG_MODE
        response["verify_code"] = Verify_response.verify_code();
#endif
        boost::beast::ostream(connection->getResponse().body()) << response.dump();
    }

    void handle_post_register(const std::shared_ptr<HttpConnection> &connection) {
        std::string body = boost::beast::buffers_to_string(connection->getRequest().body().data());
        nlohmann::json json_data;
        nlohmann::json response;

        connection->getResponse().set(http::field::content_type, "application/json");
        try {
            json_data = nlohmann::json::parse(body);
        }
        catch (const nlohmann::json::parse_error &e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            response["error"] = ErrorCode::JSON_PARSE_ERROR;
            response["message"] = "Invalid JSON format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        if (!json_data.contains("email") || !json_data["email"].is_string()) {
            std::cerr << "Invalid JSON format: missing or invalid 'email'" << std::endl;
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid parameter: 'email' is required and must be a string";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string email = json_data["email"].get<std::string>();
        if (!isValidEmail(email)) {
            std::cerr << "Invalid email format: " << email << std::endl;
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid email format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        if (!json_data.contains("verify_code") || !json_data["verify_code"].is_string()) {
            std::cerr << "Invalid JSON format: missing or invalid 'verify_code'" << std::endl;
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid parameter: 'verify_code' is required and must be a string";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string verify_code = json_data["verify_code"].get<std::string>();

        if (!json_data.contains("username") || !json_data["username"].is_string()) {
            std::cerr << "Invalid JSON format: missing or invalid 'username'" << std::endl;
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid parameter: 'username' is required and must be a string";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string username = json_data["username"].get<std::string>();
        if (!isValidUsername(username)) {
            std::cerr << "Invalid username format: " << username << std::endl;
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid username format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        if (!json_data.contains("password") || !json_data["password"].is_string()) {
            std::cerr << "Invalid JSON format: missing or invalid 'password'" << std::endl;
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid parameter: 'password' is required and must be a string";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string password = json_data["password"].get<std::string>();
        if (!isValidPassword(password)) {
            std::cerr << "Invalid password format: " << password << std::endl;
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid password format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        auto redis_mgr = RedisMgr::getInstance();
        // 检查验证码是否正确
        if (!redis_mgr->exists("verify_code_" + email)
            || redis_mgr->get("verify_code_" + email) != verify_code) {
            response["error"] = ErrorCode::VERIFY_CODE_ERROR;
            response["message"] = "Verify code error";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        // 检查 Redis 缓存中是否存在该邮箱
        if (redis_mgr->exists("email_" + email)) {
            response["error"] = ErrorCode::EMAIL_EXISTS;
            response["message"] = "Email already registered";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        // 检查 Redis 缓存中是否存在该用户名
        if (redis_mgr->exists("username_" + username)) {
            response["error"] = ErrorCode::USERNAME_EXISTS;
            response["message"] = "Username already registered";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        int uid = MysqlMgr::getInstance()->registerUser(username, email, password);
        if (uid == 0) {
            response["error"] = ErrorCode::MYSQL_ERROR;
            response["message"] = "MySQL error";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }
        if (uid == -1 || uid == -2) {
            response["error"] = uid == -1 ? ErrorCode::USERNAME_EXISTS : ErrorCode::EMAIL_EXISTS;
            response["message"] = uid == -1 ? "Username already registered" : "Email already registered";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        // 注册成功，将邮箱和用户名存入 Redis 24 小时
        redis_mgr->set("email_" + email, std::to_string(uid), 86400);
        redis_mgr->set("username_" + username, std::to_string(uid), 86400);

        // 注册成功，返回成功响应
        response["error"] = ErrorCode::SUCCESS;
        response["message"] = "Registration successful";
        boost::beast::ostream(connection->getResponse().body()) << response.dump();
        // 删除验证码
        redis_mgr->del("verify_code_" + email);
    }

} // RequestHandlerFuncs

#endif //GATESERVER_REQUESTHANDLERS_H
