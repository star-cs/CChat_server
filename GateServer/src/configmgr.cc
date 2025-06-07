/*
 * @Author: star-cs
 * @Date: 2025-06-06 21:20:27
 * @LastEditTime: 2025-06-07 17:36:24
 * @FilePath: /CChat_server/GateServer/src/configmgr.cc
 * @Description: 
 */
#include "configmgr.h"

namespace core
{
    ConfigMgr &ConfigMgr::GetInstance()
    {
        static ConfigMgr config_mgr;
        return config_mgr;
    }

    ConfigMgr::ConfigMgr()
    {
        load("config.ini");
    }

    bool ConfigMgr::load(const std::string &filename)
    {
        try
        {
            // boost::filesystem::path current_path = boost::filesystem::current_path();   // 命令行运行的路径
            boost::property_tree::ptree pt;

            // 直接返回可执行文件的根目录
            boost::filesystem::path current_path = boost::dll::program_location().parent_path();
            boost::filesystem::path config_path = current_path / filename;
            std::cout << "--------- config.ini path:" << config_path << " ---------"
                      << std::endl;
            boost::property_tree::ini_parser::read_ini(config_path.string(), pt);
            /**
            ptree
            ├── database
            │    ├── host = "127.0.0.1"
            │    └── port = "3306"
            └── server
                └── threads = "8"
            也就是
            map<string, map<string, string> >

             */

            _config_map.clear();

            for (const auto &section : pt)
            {
                SectionInfo sectionInfo;
                for (const auto &key_value : section.second)
                {
                    sectionInfo._section_datas[key_value.first] = key_value.second.get_value<std::string>();
                }
                _config_map[section.first] = sectionInfo;
            }
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to load config: " << e.what() << std::endl;
            return false;
        }
    }

}