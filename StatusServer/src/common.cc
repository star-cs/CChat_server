/*
 * @Author: star-cs
 * @Date: 2025-06-06 22:04:53
 * @LastEditTime: 2025-06-08 16:06:54
 * @FilePath: /CChat_server/GateServer/src/common.cc
 * @Description:
 */
#include "common.h"

namespace core
{
    unsigned char ToHex(unsigned char x)
    {
        // 0 --> ASCCI 48
        // 10 -> A 65
        return x > 9 ? x + 55 : x + 48;
    }

    unsigned char FromHex(unsigned char x)
    {
        unsigned char y;
        if (x >= 'A' && x <= 'Z')
            y = x - 'A' + 10;
        else if (x >= 'a' && x <= 'z')
            y = x - 'a' + 10;
        else if (x >= '0' && x <= '9')
            y = x - '0';
        else
            assert(0);
        return y;
    }

    std::string UrlEncode(const std::string &str)
    {
        std::string strTemp = "";
        std::size_t length = str.length();
        for (std::size_t i = 0; i < length; i++)
        {
            // 判断是否仅有数字和字母构成
            if (isalnum((unsigned char)str[i]) ||
                (str[i] == '-') ||
                (str[i] == '_') ||
                (str[i] == '.') ||
                (str[i] == '~'))
                strTemp += str[i];
            else if (str[i] == ' ') // 为空字符
                strTemp += "+";
            else
            {
                // 其他字符需要提前加%并且高四位和低四位分别转为16进制
                strTemp += '%';
                strTemp += ToHex((unsigned char)str[i] >> 4);
                strTemp += ToHex((unsigned char)str[i] & 0x0F);
            }
        }
        return strTemp;
    }

    std::string UrlDecode(const std::string &str)
    {
        std::string strTemp = "";
        std::size_t length = str.length();
        for (std::size_t i = 0; i < length; i++)
        {
            // 还原+为空
            if (str[i] == '+')
                strTemp += ' ';
            // 遇到%将后面的两个字符从16进制转为char再拼接
            else if (str[i] == '%')
            {
                assert(i + 2 < length);
                unsigned char high = FromHex((unsigned char)str[++i]);
                unsigned char low = FromHex((unsigned char)str[++i]);
                strTemp += high * 16 + low;
            }
            else
                strTemp += str[i];
        }
        return strTemp;
    }
}