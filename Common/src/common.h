/*
 * @Author: star-cs
 * @Date: 2025-06-30 11:24:36
 * @LastEditTime: 2025-07-07 23:00:50
 * @FilePath: /CChat_server/Common/src/common.h
 * @Description: 
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

#define MAX_LENGTH 1024 * 2
// 头部总长度
#define HEAD_TOTAL_LEN 4
// 头部id长度
#define HEAD_ID_LEN 2
// 头部数据长度
#define HEAD_DATA_LEN 2
#define MAX_RECVQUE 10000

// CSession 发送队列 最大值
#define MAX_SENDQUE 1000

#define USERIPPREFIX "uip_"
#define USERTOKENPREFIX "utoken_"
#define IPCOUNTPREFIX "ipcount_"
#define USER_BASE_INFO "ubaseinfo_"
#define LOGIN_COUNT "logincount"
#define NAME_INFO "nameinfo_"
#define LOCK_PREFIX "lock_"
#define USER_SESSION_PREFIX "usession_"
#define LOCK_COUNT "lockcount"

//分布式锁的持有时间
#define LOCK_TIME_OUT 10
//分布式锁的重试时间
#define ACQUIRE_TIME_OUT 5

// 心跳机制 超时时间 单位秒
#define HEARTBEAT_TIMEOUT 20

namespace core
{

// char 转为16进制，10->a，
extern unsigned char ToHex(unsigned char x);

// 16进制转为十进制char
extern unsigned char FromHex(unsigned char x);

// url编码，空字符用'+'拼接，复杂字符（例如中文）'%'和两个十六进制字符拼接（高四位，低四位分别拼接）。
extern std::string UrlEncode(const std::string &str);

extern std::string UrlDecode(const std::string &str);

extern bool isPureDigit(const std::string &str);

enum ErrorCodes {
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
    CREATE_CHAT_FAILED = 1012,  // 创建新聊天会话失败。
};

// Defer类
class Defer
{
public:
    // 接受一个lambda表达式或者函数指针
    Defer(std::function<void()> func) : func_(func) {}

    // 析构函数中执行传入的函数
    ~Defer() { func_(); }

private:
    std::function<void()> func_;
};

enum MSG_IDS {
    ID_CHAT_LOGIN = 1005,               //用户登陆
    ID_CHAT_LOGIN_RSP = 1006,           //用户登陆回包
    ID_SEARCH_USER_REQ = 1007,          //用户搜索请求
    ID_SEARCH_USER_RSP = 1008,          //搜索用户回包
    ID_ADD_FRIEND_REQ = 1009,           //申请添加好友请求
    ID_ADD_FRIEND_RSP = 1010,           //申请添加好友回复
    ID_NOTIFY_ADD_FRIEND_REQ = 1011,    //通知用户添加好友申请
    ID_AUTH_FRIEND_REQ = 1013,          //认证好友请求
    ID_AUTH_FRIEND_RSP = 1014,          //认证好友回复
    ID_NOTIFY_AUTH_FRIEND_REQ = 1015,   //通知用户认证好友申请
    ID_TEXT_CHAT_MSG_REQ = 1017,        //文本聊天信息请求
    ID_TEXT_CHAT_MSG_RSP = 1018,        //文本聊天信息回复
    ID_NOTIFY_TEXT_CHAT_MSG_REQ = 1019, //通知用户文本聊天信息
    ID_NOTIFY_OFF_LINE_REQ = 1021,      //通知用户下线
    ID_HEART_BEAT_REQ = 1023,           //心跳请求
    ID_HEART_BEAT_RSP = 1024,           //心跳回复
    ID_LOAD_CHAT_THREAD_REQ = 1025,     //加载聊天线程请求
    ID_LOAD_CHAT_THREAD_RSP = 1026,     //加载聊天线程回复
    ID_CREATE_PRIVATE_CHAT_REQ = 1027,  //创建私聊请求
    ID_CREATE_PRIVATE_CHAT_RSP = 1028,  //创建私聊回复
};

struct UserInfo {
    UserInfo() : name(""), pwd(""), uid(0), email(""), nick(""), desc(""), sex(0), icon("") {}
    std::string name;
    std::string pwd;
    int uid;
    std::string email;
    std::string nick;
    std::string desc;
    int sex;
    std::string icon;
};

struct ApplyInfo {
    ApplyInfo(int uid, std::string name, std::string desc, std::string icon, std::string nick,
              int sex, int status)
        : _uid(uid), _name(name), _desc(desc), _icon(icon), _nick(nick), _sex(sex), _status(status)
    {
    }

    int _uid;
    std::string _name;
    std::string _desc;
    std::string _icon;
    std::string _nick;
    int _sex;
    int _status;
};

//聊天线程会话信息
struct ChatThreadInfo {
    int _thread_id;
    std::string _type; // "private" or "group"
    int _user1_id;     // 私聊时对应 private_chat.user1_id；群聊时设为 0
    int _user2_id;     // 私聊时对应 private_chat.user2_id；群聊时设为 0
};

struct AddFriendmsg{
    int sender_id ;
    std::string unique_id;
    int msg_id;
    int thread_id;
    std::string msgcontent;
};

} // namespace core
