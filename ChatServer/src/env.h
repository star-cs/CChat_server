#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "singleton.h"

namespace core
{
class Env : public Singleton<Env>
{
    friend Singleton<Env>;

public:
    // 解析命令行参数
    void parse(int argc, char **argv);

    // 获取原始命令行参数
    const std::vector<std::string> &args() const;

    // 获取选项值（带-或--前缀的）
    std::string getOption(const std::string &name, const std::string &defaultValue = "") const;

    // 检查选项是否存在
    bool hasOption(const std::string &name) const;

    // 获取位置参数（非选项参数）
    std::vector<std::string> positionalArgs() const;

private:
    std::vector<std::string> argv_;
    std::unordered_map<std::string, std::string> options_;
};
} // namespace core
