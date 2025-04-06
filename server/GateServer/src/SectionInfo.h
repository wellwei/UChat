//
// Created by wellwei on 2025/4/4.
//

#ifndef GATESERVER_SECTIONINFO_H
#define GATESERVER_SECTIONINFO_H

#include <unordered_map>
#include <string>

struct SectionInfo {
    SectionInfo() = default;

    ~SectionInfo() {
        section_datas_.clear();
    }

    SectionInfo(const SectionInfo &src) {
        section_datas_ = src.section_datas_;
    }

    SectionInfo &operator=(const SectionInfo &src) {
        if (this != &src) {
            section_datas_ = src.section_datas_;
        }
        return *this;
    }

    std::unordered_map<std::string, std::string> section_datas_;

    std::string operator[](const std::string &key) {
        auto it = section_datas_.find(key);
        if (it != section_datas_.end()) {
            return it->second;
        }
        return "";
    }
};

#endif //GATESERVER_SECTIONINFO_H
