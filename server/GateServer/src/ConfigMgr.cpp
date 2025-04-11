//
// Created by wellwei on 2025/4/4.
//

#include "ConfigMgr.h"
#include "global.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/filesystem.hpp>

ConfigMgr::ConfigMgr() {
    // 获取配置文件路径
    boost::filesystem::path config_path = boost::filesystem::current_path() / "config.ini";
    if (!boost::filesystem::exists(config_path)) {
        std::cerr << "Config file not found: " << config_path.string() << std::endl;
        return;
    }

    // 读取配置文件
    boost::property_tree::ptree pt;
    boost::property_tree::read_ini(config_path.string(), pt);

    // 遍历配置文件的每个 section
    for (const auto &section: pt) {
        const std::string &section_name = section.first;
        const boost::property_tree::ptree &section_tree = section.second;

        // 遍历 section 中的每个 key-value 对
        std::unordered_map<std::string, std::string> section_config;
        for (const auto &key_value: section_tree) {
            const std::string &key = key_value.first;
            const std::string &value = key_value.second.get_value<std::string>();
            section_config[key] = value;
        }

        SectionInfo section_info;
        section_info.section_datas_ = section_config;
        config_map_[section_name] = section_info;
    }

    // 打印配置文件内容
    /*
    for (const auto &section: config_map_) {
        const std::string &section_name = section.first;
        const SectionInfo &section_info = section.second;
        std::cout << "[" << section_name << "]" << std::endl;
        for (const auto &key_value: section_info.section_datas_) {
            std::cout << key_value.first << " = " << key_value.second << std::endl;
        }
    }
     */
}
