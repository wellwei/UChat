//
// Created by wellwei on 2025/4/4.
//

#ifndef GATESERVER_CONFIGMGR_H
#define GATESERVER_CONFIGMGR_H


#include "SectionInfo.h"
#include "Singleton.h"

/**
 * ConfigMgr 类，
 * 全局配置管理器，负责管理配置文件的读取和写入
 */
class ConfigMgr : public Singleton<ConfigMgr> {
    friend class Singleton<ConfigMgr>;

public:
    ~ConfigMgr() {
        config_map_.clear();
    }

    SectionInfo operator[](const std::string &section) {
        if (config_map_.find(section) == config_map_.end()) {
            config_map_[section] = SectionInfo();
        }
        return config_map_[section];
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


#endif //GATESERVER_CONFIGMGR_H
