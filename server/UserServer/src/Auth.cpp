#include "Auth.h"
#include "ConfigMgr.h"
#include <jwt-cpp/jwt.h>
#include <chrono>

Auth::Auth() {
    auto config = *ConfigMgr::getInstance();
    secret_key = config["JWT"]["secret_key"];
    token_expiration = std::chrono::seconds(std::stoul(config["JWT"]["expiration"]));
}

std::string Auth::generateToken(uint64_t uid) {
    auto algorithm = jwt::algorithm::hs256(secret_key);
    auto token = jwt::create()
        .set_type("JWT")
        .set_issuer("UserService")
        .set_issued_at(std::chrono::system_clock::now())    
        .set_expires_at(std::chrono::system_clock::now() + token_expiration)
        .set_subject(std::to_string(uid))
        .sign(algorithm);
    LOG_DEBUG("Auth::generateToken() 生成token: {}", token);
    return token;
}

bool Auth::verifyToken(const std::string& token, uint64_t& uid) {
    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256(secret_key))
            .with_issuer("UserService");
        verifier.verify(decoded);
        
        try {
            uid = std::stoull(decoded.get_subject());
        } catch (const std::exception& e) {
            LOG_ERROR("Auth::verifyToken() 验证失败: {}", e.what());
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Auth::verifyToken() 验证失败: {}", e.what());    
    }
    return false;
}