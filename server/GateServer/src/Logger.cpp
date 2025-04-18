//
// Created by wellwei on 2025/4/14.
//

#include "Logger.h"

#include <utility>
#include "global.h"

TimeAndSizeRollingFileSink::TimeAndSizeRollingFileSink(std::string base_filename, size_t max_size,
                                                       int max_files)
        : base_filename_(std::move(base_filename)),
          max_size_(max_size),
          max_files_(max_files),
          current_size_(0),
          roll_size_(0),
          current_date_(getCurrentDate()) {
    open_file();
}

TimeAndSizeRollingFileSink::~TimeAndSizeRollingFileSink() {
    if (file_stream_.is_open()) {
        file_stream_.flush();
        file_stream_.close();
    }
}

void TimeAndSizeRollingFileSink::sink_it_(const spdlog::details::log_msg &msg) {
    check_roll();

    spdlog::memory_buf_t formatted;
    this->formatter_->format(msg, formatted);
    file_stream_ << fmt::to_string(formatted);
    current_size_ += formatted.size();
}

void TimeAndSizeRollingFileSink::flush_() {
    file_stream_.flush();
}

void TimeAndSizeRollingFileSink::open_file() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }

    std::string new_filename_ = base_filename_ + "_" + getHostName() + "_" + current_date_ +
                                (roll_size_ > 0 ? "_" + std::to_string(roll_size_) : "") + ".log";
    file_stream_.open(new_filename_, std::ios::app);
    if (!file_stream_) {
        throw std::runtime_error("Failed to open log file: " + new_filename_);
    }
    current_size_ = file_stream_.tellp();
}

void TimeAndSizeRollingFileSink::check_roll() {
    std::string today = getCurrentDate();
    if (today != current_date_ || current_size_ >= max_size_) {
        file_stream_.close();
        if (current_date_ != today) {
            current_date_ = today;
            roll_size_ = 0;
        } else {
            roll_size_++;
        }
        open_file();
    }
}

void Logger::init(const std::string &logDir, const std::string &filename) {
    try {

        if (!logDir.empty()) {
            std::filesystem::path dir(logDir);
            if (!std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir);
            }
        }

        auto file_sink = std::make_shared<TimeAndSizeRollingFileSink>(logDir + "/" + filename, 1024 * 1024 * 10, 5);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();
        dist_sink->add_sink(file_sink);
        dist_sink->add_sink(console_sink);

        spdlog::init_thread_pool(8192, 1);
        auto main_logger = std::make_shared<spdlog::async_logger>("main_logger", dist_sink, spdlog::thread_pool(),
                                                                  spdlog::async_overflow_policy::block);
        main_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#]: %v");
        main_logger->set_level(spdlog::level::info);
        spdlog::set_default_logger(main_logger);

        spdlog::flush_on(spdlog::level::err);      // 发生 error 级别的日志时立即刷新
        spdlog::flush_every(std::chrono::seconds(3));   // 每 3 秒刷新一次

        LOG_INFO("Logger initialized successfully.");
    }
    catch (const std::exception &e) {
        std::cerr << "Logger initialization failed: " << e.what() << std::endl;
        throw;
    }
}

void Logger::setLogLevel(spdlog::level::level_enum level) {
    if (spdlog::default_logger()) {
        spdlog::set_level(level);
    }
}

void Logger::flush() {
    if (spdlog::default_logger()) {
        spdlog::default_logger()->flush();
    }
}