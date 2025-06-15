#pragma once

#include <string>
#include <unordered_map>
#include <iostream>
#include "Singleton.h"

struct SectionInfo {
    std::unordered_map<std::string, std::string> section_datas;

    SectionInfo() = default;

    SectionInfo(const SectionInfo& src) {
        section_datas = src.section_datas;
    }

    SectionInfo& operator=(const SectionInfo& src) {
        if (this != &src) {
            section_datas = src.section_datas;
        }
        return *this;
    }

    std::string& operator[](const std::string& key) {
        return section_datas[key];
    }

    const std::string& operator[](const std::string& key) const {
        static const std::string empty_string;
        auto it = section_datas.find(key);
        if (it == section_datas.end()) {
            return empty_string;
        }
        return it->second;
    }

    const std::string& get(const std::string& key, const std::string& default_value = "") const {
        auto it = section_datas.find(key);
        if (it == section_datas.end()) {
            return default_value;
        }
        return it->second;
    }
};

class ConfigMgr : public Singleton<ConfigMgr> {
    friend class Singleton<ConfigMgr>;

public:
    ~ConfigMgr() {
        config_map_.clear();
    }

    SectionInfo& operator[](const std::string &section) {
        return config_map_[section];
    }

    const SectionInfo& operator[](const std::string &section) const {
        static const SectionInfo empty_section;
        auto it = config_map_.find(section);
        if (it == config_map_.end()) {
            return empty_section;
        }
        return it->second;
    }

    ConfigMgr &operator=(const ConfigMgr &src) {
        if (this != &src) {
            config_map_ = src.config_map_;
        }
        return *this;
    }

    ConfigMgr(const ConfigMgr &src) {
        this->config_map_ = src.config_map_;
    }

private:
    ConfigMgr();

    std::unordered_map<std::string, SectionInfo> config_map_;
};