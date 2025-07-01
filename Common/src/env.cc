#include "env.h"
#include <algorithm>
#include <cctype>

namespace core
{
    void Env::parse(int argc, char **argv)
    {
        // 保存原始参数
        for (int i = 0; i < argc; ++i)
        {
            argv_.emplace_back(argv[i]);
        }

        // 解析选项
        for (size_t i = 1; i < argv_.size(); ++i)
        {
            const std::string &arg = argv_[i];

            // 长选项 (--option=value)
            if (arg.find("--") == 0)
            {
                size_t pos = arg.find('=');
                std::string key = arg.substr(2, pos - 2);
                if (pos != std::string::npos)
                {
                    options_[key] = arg.substr(pos + 1);
                }
                else
                {
                    options_[key] = ""; // 标记存在但无值
                }
            }
            // 短选项 (-x value 或 -xvalue)
            else if (arg.find('-') == 0)
            {
                std::string key = arg.substr(1);

                // 处理组合短选项 (-abc)
                if (key.length() > 1 && !std::isdigit(key[0]))
                {
                    for (char c : key)
                    {
                        options_[std::string(1, c)] = "";
                    }
                }
                // 正常短选项
                else
                {
                    if (i + 1 < argv_.size() && argv_[i + 1][0] != '-')
                    {
                        options_[key] = argv_[i + 1];
                        ++i; // 跳过值
                    }
                    else
                    {
                        options_[key] = "";
                    }
                }
            }
        }
    }

    const std::vector<std::string> &Env::args() const
    {
        return argv_;
    }

    std::string Env::getOption(const std::string &name, const std::string &defaultValue) const
    {
        auto it = options_.find(name);
        if (it != options_.end())
        {
            return it->second;
        }
        return defaultValue;
    }

    bool Env::hasOption(const std::string &name) const
    {
        return options_.find(name) != options_.end();
    }

    std::vector<std::string> Env::positionalArgs() const
    {
        std::vector<std::string> result;
        for (size_t i = 1; i < argv_.size(); ++i)
        {
            const std::string &arg = argv_[i];
            if (arg[0] != '-')
            {
                result.push_back(arg);
            }
            // 跳过带值的选项
            else if (i + 1 < argv_.size() && argv_[i + 1][0] != '-' &&
                     arg.find('=') == std::string::npos)
            {
                ++i;
            }
        }
        return result;
    }

} // namespace core
