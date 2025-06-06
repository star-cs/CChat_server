#pragma once

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

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

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

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
        Error_Json = 1001, // Json解析错误
        RPCFailed = 1002,  // RPC请求错误
    };


}
