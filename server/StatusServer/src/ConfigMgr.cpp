#include "ConfigMgr.h"
#include <filesystem>
#include <fstream>
#include "Logger.h"
#include "util.h"

ConfigMgr::ConfigMgr() {
    std::filesystem::path config_path = std::filesystem::current_path() / "config.ini";
    if (!std::filesystem::exists(config_path)) {
        LOG_ERROR("配置文件不存在: {}", config_path.string());
        throw std::runtime_error("配置文件不存在");
    }

    std::ifstream config_file(config_path.string());
    if (!config_file.is_open()) {
        LOG_ERROR("无法打开配置文件: {}", config_path.string());
        throw std::runtime_error("无法打开配置文件");
    }

    // 解析配置文件，注意等号两边有空格
    std::string line;
    std::string section;
    while (std::getline(config_file, line)) {
        if (line.empty() || line[0] == ';') {
            continue;
        }
        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                section = line.substr(1, end - 1);
                config_map_[section] = SectionInfo();
            }   
        } else {
            // 解析等号两边有空格的配置项
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                // 去除等号两边的空格
                key = trim(key);
                value = trim(value);
                config_map_[section][key] = value;
            }
        }
    }
    // printConfig();
}
