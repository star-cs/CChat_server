/*
 * @Author: star-cs
 * @Date: 2025-06-06 21:13:05
 * @LastEditTime: 2025-06-22 11:11:46
 * @FilePath: /CChat_server/ChatServer/src/configmgr.h
 * @Description: ini配置文件解析 类
 */
#pragma once
#include "common.h"

namespace core
{
struct SectionInfo {
    SectionInfo() {}
    ~SectionInfo() { _section_datas.clear(); }

    SectionInfo(const SectionInfo &src) { _section_datas = src._section_datas; }
    SectionInfo &operator=(const SectionInfo &src)
    {
        if (&src == this) {
            return *this;
        }
        this->_section_datas = src._section_datas;
        return *this;
    }

    std::map<std::string, std::string> _section_datas;

    std::string operator[](const std::string &key)
    {
        if (_section_datas.find(key) == _section_datas.end()) {
            return "";
        }
        return _section_datas[key];
    }
};

class ConfigMgr
{
public:
    static ConfigMgr &GetInstance(const std::string &filename = "config.ini")
    {
        static ConfigMgr config_mgr(filename);
        return config_mgr;
    }

    ~ConfigMgr() { _config_map.clear(); }

    SectionInfo operator[](const std::string &section)
    {
        if (_config_map.find(section) == _config_map.end()) {
            return SectionInfo();
        }
        return _config_map[section];
    }

    ConfigMgr &operator=(const ConfigMgr &src) = delete;

    ConfigMgr(const ConfigMgr &src) = delete;

private:
    ConfigMgr(const std::string &filename);

    bool load(const std::string &filename);

private:
    std::map<std::string, SectionInfo> _config_map;
};

} // namespace core
