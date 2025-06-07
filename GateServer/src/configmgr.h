/*
 * @Author: star-cs
 * @Date: 2025-06-06 21:13:05
 * @LastEditTime: 2025-06-07 20:09:17
 * @FilePath: /CChat_server/GateServer/src/configmgr.h
 * @Description: ini配置文件解析 类
 */
#pragma once
#include "common.h"

namespace core
{
    struct SectionInfo
    {
        SectionInfo() {}
        ~SectionInfo()
        {
            _section_datas.clear();
        }

        SectionInfo(const SectionInfo &src)
        {
            _section_datas = src._section_datas;
        }
        SectionInfo &operator=(const SectionInfo &src)
        {
            if (&src == this)
            {
                return *this;
            }
            this->_section_datas = src._section_datas;
            return *this;
        }

        std::map<std::string, std::string> _section_datas;

        std::string operator[](const std::string &key)
        {
            if (_section_datas.find(key) == _section_datas.end())
            {
                return "";
            }
            return _section_datas[key];
        }
    };

    class ConfigMgr
    {
    public:
        static ConfigMgr& GetInstance();

        ~ConfigMgr()
        {
            _config_map.clear();
        }

        SectionInfo operator[](const std::string &section)
        {
            if (_config_map.find(section) == _config_map.end())
            {
                return SectionInfo();
            }
            return _config_map[section];
        }

        ConfigMgr &operator=(const ConfigMgr &src)
        {
            if (&src == this)
            {
                return *this;
            }
            this->_config_map = src._config_map;
            return *this;
        }

        ConfigMgr(const ConfigMgr &src)
        {
            this->_config_map = src._config_map;
        }
    private:
        ConfigMgr();

        bool load(const std::string &filename);

    private:
        std::map<std::string, SectionInfo> _config_map;
    };

} // namespace core
