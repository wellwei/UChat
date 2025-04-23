#pragma once

#include <string>
#include <jwt-cpp/jwt.h>
#include "Singleton.h"
#include "Logger.h"

class Auth : public Singleton<Auth> {
    friend class Singleton<Auth>;
public:
    ~Auth() = default;

    bool verifyToken(const std::string& token, uint64_t& uid);
    std::string generateToken(uint64_t uid);

private:
    Auth();
    std::string secret_key; 
    std::chrono::seconds token_expiration;
};

