#include "RequestHandlers.h"
#include "global.h"
#include "HttpConnection.h"
#include "util.h"
#include "RedisMgr.h"
#include "VerifyGrpcClient.h"
#include "Logger.h"
#include "Singleton.h"
#include "StatusGrpcClient.h"
#include "UserServiceClient.h"
#include <jwt-cpp/jwt.h>
#include "ConfigMgr.h"

using json = nlohmann::json;

namespace {

    // 封装响应发送
    void send_json_response(const std::shared_ptr<HttpConnection> &connection, const json &response_json, http::status status = http::status::ok) {
        auto &response = connection->getResponse();
        response.result(status);
        response.set(http::field::content_type, "application/json");
        boost::beast::ostream(response.body()) << response_json.dump();
    }

    // 统一的错误响应生成
    json make_error_response(ErrorCode error_code, const std::string &message) {
        LOG_WARN("Error Response: code={}, message={}", static_cast<int>(error_code), message);
        return {
            {"error", error_code},
            {"message", message}
        };
    }

    // 验证JWT令牌并检查UID是否匹配
    bool verify_jwt_token(const std::string& token, const uint64_t uid, nlohmann::json& error_response) {
        try {
            auto& config = *ConfigMgr::getInstance();
            const auto decoded = jwt::decode(token);
            const auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256(config["JWT"]["secret_key"]))
                .with_issuer("UserService");
            verifier.verify(decoded);
            uint64_t token_uid = std::stoull(decoded.get_subject());
            if (uid != token_uid) {
                error_response = make_error_response(ErrorCode::INVALID_PARAMETER, "Invalid UID, token mismatch.");
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Token verification failed: {}", e.what());
            error_response = make_error_response(ErrorCode::TOKEN_INVALID, "Token invalid or expired.");
            return false;
        }
    }

    // 处理常规请求包装器
    void handle_request(const std::shared_ptr<HttpConnection> &connection,
                        const std::function<json(const json&)> &logic_handler) {
        std::string body(boost::beast::buffers_to_string(connection->getRequest().body().data()));
        json request_json;
        try {
            if (!body.empty()) {
                request_json = json::parse(body);
            }
        } catch (const json::parse_error& e) {
            send_json_response(connection, make_error_response(ErrorCode::JSON_PARSE_ERROR, "Invalid JSON format."), http::status::bad_request);
            return;
        }
        auto response_json = logic_handler(request_json);
        send_json_response(connection, response_json, http::status::ok);
    }

    // 处理需要认证的请求包装器
    void handle_anthenticated_request(const std::shared_ptr<HttpConnection> &connection,
                                      const std::function<json(uint64_t, const json&)> &logic_handler) {
        
        handle_request(connection, [&](const json& request_json) {
            if (!request_json.contains("uid") || !request_json.contains("token") ||
                !request_json["uid"].is_string() || !request_json["token"].is_string()) {
                return make_error_response(ErrorCode::INVALID_PARAMETER, "Missing or invalid uid/token");
            }
            
            uint64_t uid;
            try {
                uid = std::stoull(request_json["uid"].get<std::string>());
            } catch (const std::exception& e) {
                return make_error_response(ErrorCode::INVALID_PARAMETER, "Invalid UID format");
            }

            std::string token = request_json["token"].get<std::string>();
            json error_response;
            if (!verify_jwt_token(token, uid, error_response)) {
                return error_response;
            }

            return logic_handler(uid, request_json);
        });
    }

    json process_verify_code(const json& request_json) {
        if (!request_json.contains("email") || !request_json["email"].is_string()) {
            return make_error_response(ErrorCode::INVALID_PARAMETER, "Missing or invalid email");
        }

        std::string email = request_json["email"].get<std::string>();
        std::string throttle_key = "throttle_verify_" + email;
        auto redis_mgr = RedisMgr::getInstance();
        if (redis_mgr->exists(throttle_key)) {
            return make_error_response(ErrorCode::TOO_MANY_REQUESTS, "Too many requests. Please try again later.");
        }

        Status Verify_response = VerifyGrpcClient::getInstance()->getVerifyCode(email);
        if (!Verify_response.ok()) {
            return make_error_response(ErrorCode::RPC_ERROR, Verify_response.error_message());
        }

        redis_mgr->set(throttle_key, "1", 60);

        return {
            {"error", ErrorCode::SUCCESS},
            {"email", email}
        };
    }

    json process_register(const json& request_json) {
        if (!request_json.contains("email") || !request_json["email"].is_string() ||
            !request_json.contains("verify_code") || !request_json["verify_code"].is_string() ||
            !request_json.contains("username") || !request_json["username"].is_string() ||
            !request_json.contains("password") || !request_json["password"].is_string()) {
            return make_error_response(ErrorCode::INVALID_PARAMETER, "Missing or invalid parameters");
        }

        std::string email = request_json["email"].get<std::string>();
        std::string verify_code = request_json["verify_code"].get<std::string>();
        std::string username = request_json["username"].get<std::string>();
        std::string password = request_json["password"].get<std::string>();

        auto redis_mgr = RedisMgr::getInstance();
        if (!redis_mgr->exists("verify_code_" + email) || redis_mgr->get("verify_code_" + email) != verify_code) {
            return make_error_response(ErrorCode::VERIFY_CODE_ERROR, "Invalid verify code");
        }

        uint64_t uid = 0;
        uint32_t code = UserServiceClient::getInstance()->CreateUser(username, password, email, uid);
        if (code == ErrorCode::USER_EXISTS) {
            return make_error_response(ErrorCode::USER_EXISTS, "User already exists");
        } else if (code != ErrorCode::SUCCESS) {
            return make_error_response(ErrorCode::RPC_ERROR, "Failed to create user");
        }

        redis_mgr->del("verify_code_" + email);

        return {
            {"error", ErrorCode::SUCCESS},
            {"message", "Registration successful"},
            {"uid", uid}
        };
    }

    json process_reset_password(const json& request_json) {
        if (!request_json.contains("email") || !request_json["email"].is_string() ||
            !request_json.contains("verify_code") || !request_json["verify_code"].is_string() ||
            !request_json.contains("new_password") || !request_json["new_password"].is_string()) {
            return make_error_response(ErrorCode::INVALID_PARAMETER, "Missing or invalid parameters");
        }

        std::string email = request_json["email"].get<std::string>();
        std::string verify_code = request_json["verify_code"].get<std::string>();
        std::string new_password = request_json["new_password"].get<std::string>();

        auto redis_mgr = RedisMgr::getInstance();
        if (!redis_mgr->exists("verify_code_" + email) || redis_mgr->get("verify_code_" + email) != verify_code) {
            return make_error_response(ErrorCode::VERIFY_CODE_ERROR, "Invalid verify code");
        }

        if (!UserServiceClient::getInstance()->ResetPassword(email, new_password)) {
            return make_error_response(ErrorCode::RPC_ERROR, "Failed to reset password");
        }

        redis_mgr->del("verify_code_" + email);

        return {
            {"error", ErrorCode::SUCCESS},
            {"message", "Password reset successful"}
        };
    }

    json process_login(const json& request_json) {
        if (!request_json.contains("handle") || !request_json["handle"].is_string() ||
            !request_json.contains("password") || !request_json["password"].is_string()) {
            return make_error_response(ErrorCode::INVALID_PARAMETER, "Missing or invalid parameters");
        }

        std::string handle = request_json["handle"].get<std::string>();
        std::string password = request_json["password"].get<std::string>();

        json response;
        if (!UserServiceClient::getInstance()->VerifyCredentials(handle, password, response)) {
            return make_error_response(ErrorCode::RPC_ERROR, "Failed to login");
        }

        return {
            {"error", ErrorCode::SUCCESS},
            {"message", "Login successful"},
            {"uid", response["uid"]},
            {"token", response["token"]}
        };
    }

    json process_get_chat_server(uint64_t uid, const json& request_json) {
        auto reply = StatusGrpcClient::getInstance()->getChatServer(uid);
        if (reply.code() == GetChatServerResponse_Code_OK) {
            return {
                {"error", ErrorCode::SUCCESS},
                {"message", "Success"},
                {"host", reply.host()},
                {"port", reply.port()}
            };
        }

        return make_error_response(ErrorCode::RPC_ERROR, reply.error_msg());
    }

    json process_get_user_profile(uint64_t uid, const json& request_json) {
        json response;
        if (!UserServiceClient::getInstance()->GetUserProfile(uid, response)) {
            return make_error_response(ErrorCode::RPC_ERROR, "Failed to get user profile");
        }

        return {
            {"error", ErrorCode::SUCCESS},
            {"message", "Success"},
            {"user_profile", response}
        };
    }

    json process_get_contacts(uint64_t uid, const json& request_json) {
        nlohmann::json contacts;
        if (!UserServiceClient::getInstance()->GetContacts(uid, contacts)) {
            return make_error_response(ErrorCode::RPC_ERROR, "Failed to get contacts");
        }

        return {
            {"error", ErrorCode::SUCCESS},
            {"message", "Success"},
            {"contacts", contacts}
        };
    }

    json process_search_user(const json& request_json) {
        if (!request_json.contains("keyword") || !request_json["keyword"].is_string()) {
            return make_error_response(ErrorCode::INVALID_PARAMETER, "Missing or invalid parameters");
        }

        std::string keyword = request_json["keyword"].get<std::string>();
        nlohmann::json users;
        if (!UserServiceClient::getInstance()->SearchUser(keyword, users)) {
            return make_error_response(ErrorCode::RPC_ERROR, "Failed to search user");
        }

        return {
            {"error", ErrorCode::SUCCESS},
            {"message", "Success"},
            {"users", users}
        };
    }

    json process_send_contact_request(uint64_t uid, const json& request_json) {
        if (!request_json.contains("addressee_id") || !request_json["addressee_id"].is_string() ||
            !request_json.contains("message") || !request_json["message"].is_string()) {
            return make_error_response(ErrorCode::INVALID_PARAMETER, "Missing or invalid parameters");
        }
        uint64_t addressee_id = std::stoull(request_json["addressee_id"].get<std::string>());
        std::string message = request_json["message"].get<std::string>();

        uint64_t request_id = 0;
        if (!UserServiceClient::getInstance()->SendContactRequest(uid, addressee_id, message, request_id)) {
            return make_error_response(ErrorCode::RPC_ERROR, "Failed to send contact request");
        }

        return {
            {"error", ErrorCode::SUCCESS},
            {"message", "Contact request sent successfully"},
            {"request_id", request_id}
        };
    }

    json process_handle_contact_request(uint64_t uid, const json& request_json) {
        if (!request_json.contains("request_id") || !request_json["request_id"].is_string() ||
            !request_json.contains("action") || !request_json["action"].is_number_integer()) {
            return make_error_response(ErrorCode::INVALID_PARAMETER, "Missing or invalid parameters");
        }

        uint64_t request_id = std::stoull(request_json["request_id"].get<std::string>());
        int action = request_json["action"].get<int>();
        
        if (!UserServiceClient::getInstance()->HandleContactRequest(request_id, uid, action)) {
            return make_error_response(ErrorCode::RPC_ERROR, "Failed to handle contact request");
        }

        return {
            {"error", ErrorCode::SUCCESS},
            {"message", "Contact request handled successfully"},
            {"request_id", request_id},
            {""}
        };
    }

    json process_get_contact_requests(uint64_t uid, const json& request_json) {
        if (!request_json.contains("status") || !request_json["status"].is_number_integer()) {
            return make_error_response(ErrorCode::INVALID_PARAMETER, "Missing or invalid parameters");
        }

        int status = request_json["status"].get<int>();
        nlohmann::json requests;
        if (!UserServiceClient::getInstance()->GetContactRequests(uid, status, requests)) {
            return make_error_response(ErrorCode::RPC_ERROR, "Failed to get contact requests");
        }

        return {
            {"error", ErrorCode::SUCCESS},
            {"message", "Success"},
            {"requests", requests}
        };
    }
} // namespace

namespace RequestHandlerFuncs {

    void handle_get_test(const std::shared_ptr<HttpConnection> &connection) {
        boost::beast::ostream(connection->getRequest().body()) << "GET request received!";

        int i = 0;
        for (const auto &param: connection->getParams()) {
            beast::ostream(connection->getResponse().body()) << "\n" << i++ << ": " << param.first << " = "
                                                             << param.second;
        }
    }

    void handle_post_getverifycode(const std::shared_ptr<HttpConnection> &connection) {
        handle_request(connection, process_verify_code);
    }

    void handle_post_register(const std::shared_ptr<HttpConnection> &connection) {
        handle_request(connection, process_register);
    }

    void handle_post_resetPassword(const std::shared_ptr<HttpConnection> &connection) {
        handle_request(connection, process_reset_password);
    }

    void handle_post_login(const std::shared_ptr<HttpConnection> &connection) {
        handle_request(connection, process_login);
    }

    void handle_post_getChatServer(const std::shared_ptr<HttpConnection> &connection) {
        handle_anthenticated_request(connection, process_get_chat_server);
    }

    void handle_post_getUserProfile(const std::shared_ptr<HttpConnection> &connection) {
        handle_anthenticated_request(connection, process_get_user_profile);
    }


    void handle_post_getContacts(const std::shared_ptr<HttpConnection> &connection) {
        handle_anthenticated_request(connection, process_get_contacts);
    }

    void handle_post_searchUser(const std::shared_ptr<HttpConnection> &connection) {
        handle_request(connection, process_search_user);
    }

    void handle_post_sendContactRequest(const std::shared_ptr<HttpConnection> &connection) {
        handle_anthenticated_request(connection, process_send_contact_request);
    }

    void handle_post_handleContactRequest(const std::shared_ptr<HttpConnection> &connection) {
        handle_anthenticated_request(connection, process_handle_contact_request);
    }

    void handle_post_getContactRequests(const std::shared_ptr<HttpConnection> &connection) {
        handle_anthenticated_request(connection, process_get_contact_requests);
    }
} // RequestHandlerFuncs