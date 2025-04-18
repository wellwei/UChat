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
#include "Logger.h"
#include "Singleton.h"
#include "StatusGrpcClient.h"

inline namespace RequestHandlerFuncs {

    inline bool validate_json(const nlohmann::json &json_data, const std::string &key, const std::string &type,
                              nlohmann::json &response) {
        if (!json_data.contains(key) || !json_data[key].is_string()) {
            LOG_WARN(("Invalid JSON format: '{}' is required and must be a string"), key);
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid parameter: '" + key + "' is required and must be a string";
            return false;
        }
        if (json_data[key].empty()) {
            LOG_WARN(("Invalid JSON format: '{}' cannot be empty"), key);
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "'" + key + "' cannot be empty";
            return false;
        }
        std::string value = json_data[key].get<std::string>();
        if (type == "email" && !isValidEmail(value)) {
            LOG_WARN("Invalid email format: {}", value);
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid email format";
            return false;
        } else if (type == "username" && !isValidUsername(value)) {
            LOG_WARN("Invalid username format: {}", value);
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid username format";
            return false;
        } else if (type == "password" && !isValidPassword(value)) {
            LOG_WARN("Invalid password format: {}", value);
            response["error"] = ErrorCode::INVALID_PARAMETER;
            response["message"] = "Invalid password format";
            return false;
        }
        return true;
    }

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
            LOG_ERROR("JSON parse error: {}", e.what());
            response["error"] = ErrorCode::JSON_PARSE_ERROR;
            response["message"] = "Invalid JSON format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        if (!validate_json(json_data, "email", "email", response)) {
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string email = json_data["email"].get<std::string>();
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
            LOG_ERROR("JSON parse error: {}", e.what());
            response["error"] = ErrorCode::JSON_PARSE_ERROR;
            response["message"] = "Invalid JSON format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        if (!validate_json(json_data, "email", "email", response) ||
            !validate_json(json_data, "verify_code", "string", response) ||
            !validate_json(json_data, "username", "username", response) ||
            !validate_json(json_data, "password", "password", response)) {
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string email = json_data["email"].get<std::string>();
        std::string verify_code = json_data["verify_code"].get<std::string>();
        std::string username = json_data["username"].get<std::string>();
        std::string password = json_data["password"].get<std::string>();

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

    void handle_put_resetPassword(const std::shared_ptr<HttpConnection> &connection) {
        std::string body = boost::beast::buffers_to_string(connection->getRequest().body().data());
        nlohmann::json json_data;
        nlohmann::json response;

        connection->getResponse().set(http::field::content_type, "application/json");
        try {
            json_data = nlohmann::json::parse(body);
        }
        catch (const nlohmann::json::parse_error &e) {
            LOG_ERROR("JSON parse error: {}", e.what());
            response["error"] = ErrorCode::JSON_PARSE_ERROR;
            response["message"] = "Invalid JSON format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        if (!validate_json(json_data, "email", "email", response) ||
            !validate_json(json_data, "verify_code", "string", response) ||
            !validate_json(json_data, "new_password", "new_password", response)) {
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string email = json_data["email"].get<std::string>();
        std::string verify_code = json_data["verify_code"].get<std::string>();
        std::string password = json_data["new_password"].get<std::string>();
        auto redis_mgr = RedisMgr::getInstance();
        auto mysql_mgr = MysqlMgr::getInstance();

        // 检查验证码是否正确
        if (!redis_mgr->exists("verify_code_" + email)
            || redis_mgr->get("verify_code_" + email) != verify_code) {
            response["error"] = ErrorCode::VERIFY_CODE_ERROR;
            response["message"] = "Verify code error";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        // 检查 数据库中是否存在该邮箱
        int uid = mysql_mgr->getUidByEmail(email);
        if (uid <= 0) {
            response["error"] = ErrorCode::EMAIL_NOT_REGISTERED;
            response["message"] = "Email not registered";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        // 邮箱存在，重置密码
        if (!mysql_mgr->resetPassword(uid, password)) {
            response["error"] = ErrorCode::MYSQL_ERROR;
            response["message"] = "MySQL error";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        // 重置密码成功，返回成功响应
        response["error"] = ErrorCode::SUCCESS;
        response["message"] = "Password reset successful";
        boost::beast::ostream(connection->getResponse().body()) << response.dump();
        // 删除验证码
        redis_mgr->del("verify_code_" + email);
    }

    void handle_post_login(const std::shared_ptr<HttpConnection> &connection) {
        std::string body = boost::beast::buffers_to_string(connection->getRequest().body().data());
        nlohmann::json json_data;
        nlohmann::json response;

        connection->getResponse().set(http::field::content_type, "application/json");
        try {
            json_data = nlohmann::json::parse(body);
        }
        catch (const nlohmann::json::parse_error &e) {
            LOG_ERROR("JSON parse error: {}", e.what());
            response["error"] = ErrorCode::JSON_PARSE_ERROR;
            response["message"] = "Invalid JSON format";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        std::string username = json_data["username"].get<std::string>();
        std::string password = json_data["password"].get<std::string>();
        if (username.empty() || password.empty()) {
            response["error"] = ErrorCode::USERNAME_OR_PASSWORD_ERROR;
            response["message"] = "Username or password error";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

        auto mysql_mgr = MysqlMgr::getInstance();

        // 检查 数据库中是否存在该用户名
        int uid = mysql_mgr->getUidByUsername(username);
        if (uid > 0) {
            // 用户名存在，检查密码
            if (!mysql_mgr->checkPassword(uid, password)) {
                response["error"] = ErrorCode::USERNAME_OR_PASSWORD_ERROR;
                response["message"] = "Username or password error";
                boost::beast::ostream(connection->getResponse().body()) << response.dump();
                return;
            }
        } else {
            // 用户名不存在，检查邮箱
            uid = mysql_mgr->getUidByEmail(username);
            if (uid <= 0) {
                response["error"] = ErrorCode::USERNAME_OR_PASSWORD_ERROR;
                response["message"] = "Username or password error";
                boost::beast::ostream(connection->getResponse().body()) << response.dump();
                return;
            }
            // 邮箱存在，检查密码
            if (!mysql_mgr->checkPassword(uid, password)) {
                response["error"] = ErrorCode::USERNAME_OR_PASSWORD_ERROR;
                response["message"] = "Username or password error";
                boost::beast::ostream(connection->getResponse().body()) << response.dump();
                return;
            }
        }

        UserInfo user_info = mysql_mgr->getUserInfo(uid);
        if (user_info.uid <= 0) {
            response["error"] = ErrorCode::MYSQL_ERROR;
            response["message"] = "MySQL error";
            boost::beast::ostream(connection->getResponse().body()) << response.dump();
            return;
        }

//        auto reply = StatusGrpcClient::getInstance()->getChatServer(user_info.uid);

        // 登录成功，返回成功响应
        response["error"] = ErrorCode::SUCCESS;
        response["message"] = "Login successful";
        boost::beast::ostream(connection->getResponse().body()) << response.dump();
    }
} // RequestHandlerFuncs

#endif //GATESERVER_REQUESTHANDLERS_H
