//
// Created by wellwei on 2025/4/14.
//

#ifndef GATESERVER_LOGGER_H
#define GATESERVER_LOGGER_H

#include "Singleton.h"
#include <spdlog/sinks/base_sink.h>
#include <fstream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/async.h>
#include <spdlog/spdlog.h>
#include <iostream>

class TimeAndSizeRollingFileSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    TimeAndSizeRollingFileSink(std::string base_filename, size_t max_size, int max_files);

    ~TimeAndSizeRollingFileSink() override;

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override;

    void flush_() override;

private:
    std::string base_filename_;
    size_t max_size_;
    int max_files_;
    size_t current_size_;
    std::ofstream file_stream_;
    size_t roll_size_{};
    std::string current_date_;

    void open_file();

    void check_roll();
};

class Logger : public Singleton<Logger> {
    friend class Singleton<Logger>;

public:
    static void init(const std::string &logDir, const std::string &filename);

    static void setLogLevel(spdlog::level::level_enum level);

    static void flush();

    template<typename... Args>
    static void
    log(const spdlog::level::level_enum &level, const char *file, int line, fmt::format_string<Args...> fmt,
        Args &&... args) {
        auto logger = spdlog::get("main_logger");
        if (logger) {
            logger->log(spdlog::source_loc{file, line, __FUNCTION__}, level, fmt, std::forward<Args>(args)...);
        } else {
            std::cerr << "Logger not initialized" << std::endl;
        }
    }
};

// 全局日志宏

#define LOG_INFO(...) Logger::log(spdlog::level::info, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) Logger::log(spdlog::level::debug, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) Logger::log(spdlog::level::warn, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Logger::log(spdlog::level::err, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_CRITICAL(...) Logger::log(spdlog::level::critical, __FILE__, __LINE__, __VA_ARGS__)

#endif //GATESERVER_LOGGER_H
