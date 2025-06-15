/*
 * @Author: star-cs
 * @Date: 2025-06-06 09:55:25
 * @LastEditTime: 2025-06-12 11:39:21
 * @FilePath: /CChat_server/GateServer/src/common.h
 * @Description: 通用 头文件 及 工具方法，参数
 */

#pragma once

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

#include <boost/lexical_cast.hpp> // 类型转换

#include <boost/dll.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <string>
#include <functional>
#include <memory>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "logger.hpp"

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>


#define CODE_PREFIX "code_"

namespace core
{   
    
    // char 转为16进制，10->a，
    extern unsigned char ToHex(unsigned char x);

    // 16进制转为十进制char
    extern unsigned char FromHex(unsigned char x);

    // url编码，空字符用'+'拼接，复杂字符（例如中文）'%'和两个十六进制字符拼接（高四位，低四位分别拼接）。
    extern std::string UrlEncode(const std::string &str);

    extern std::string UrlDecode(const std::string &str);

    enum ErrorCodes
    {
        Success = 0,
        Error_Json = 1001,     // Json解析错误
        RPCFailed = 1002,      // RPC请求错误
        VerifyExpired = 1003,  // 验证码过期
        VerifyCodeErr = 1004,  // 验证码错误
        UserExist = 1005,      // 用户已经存在
        PasswdErr = 1006,      // 密码错误
        EmailNotMatch = 1007,  // 邮箱不匹配
        PasswdUpFailed = 1008, // 更新密码失败
        PasswdInvalid = 1009,  // 登录密码失败
        TokenInvalid = 1010,   // Token失效
        UidInvalid = 1011,     // uid无效
    };

    // Defer类
    class Defer
    {
    public:
        // 接受一个lambda表达式或者函数指针
        Defer(std::function<void()> func) : func_(func) {}

        // 析构函数中执行传入的函数
        ~Defer()
        {
            func_();
        }

    private:
        std::function<void()> func_;
    };
}
