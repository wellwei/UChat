#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include "Singleton.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>

class Logger : public Singleton<Logger> {
    friend class Singleton<Logger>;

public:
    static void init() {
        spdlog::init_thread_pool(8192, 1);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto logger = std::make_shared<spdlog::async_logger>("console", console_sink, spdlog::thread_pool());
        logger->set_level(spdlog::level::debug);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#] %v");
        spdlog::set_default_logger(logger);
    }
    template<typename... Args>
    static void log(const spdlog::level::level_enum &level, const char *file, int line, fmt::format_string<Args...> fmt,
        Args &&... args) {
        auto logger = spdlog::default_logger();
        if (logger) {
            logger->log(spdlog::source_loc{file, line, __FUNCTION__}, level, fmt, std::forward<Args>(args)...);
        } else {
            std::cerr << "Logger not initialized" << std::endl;
        }
    }

private:
    Logger() = default;
};

#define LOG_DEBUG(...) Logger::log(spdlog::level::debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) Logger::log(spdlog::level::info, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) Logger::log(spdlog::level::warn, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Logger::log(spdlog::level::err, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_CRITICAL(...) Logger::log(spdlog::level::critical, __FILE__, __LINE__, __VA_ARGS__)