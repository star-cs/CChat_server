/*
 * @Author: star-cs
 * @Date: 2025-06-16 10:00:05
 * @LastEditTime: 2025-06-25 15:52:47
 * @FilePath: /CChat_server/ChatServer/src/logic_system.cc
 * @Description:
 */
#include "logic_system.h"
#include "common.h"
#include "csession.h"
#include "redis_mgr.h"
#include "user_mgr.h"
#include "configmgr.h"
#include "mysql_mgr.h"
#include "json/config.h"
#include "json/reader.h"
#include "json/value.h"
#include <grpcpp/impl/codegen/client_context.h>
#include <memory>
#include <string>
#include <utility>
#include "grpc_client.h"

namespace core
{
LogicSystem::LogicSystem() : _b_stop(false)
{
    ResgisterCallBack();
    _worker_thread = std::thread(&LogicSystem::DelMsg, this);
}

LogicSystem::~LogicSystem()
{
    _b_stop = true;
    _cond.notify_one();
    _worker_thread.join();
}

void LogicSystem::PostMsgToQue(std::shared_ptr<LogicNode> msg)
{
    std::unique_lock<std::mutex> lock(_mutex);
    _msg_que.push(msg);
    if (_msg_que.size() == 1) {
        lock.unlock();
        _cond.notify_one();
    }
}

void LogicSystem::DelMsg()
{
    try {
        while (true) {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [this]() {
                if (_b_stop)
                    return true;
                return !_msg_que.empty();
            });

            if (_b_stop) // 关闭状态
            {
                // 把所有的 msg_node 处理掉
                while (!_msg_que.empty()) {
                    auto msg_node = _msg_que.front();
                    LOG_INFO("recv_msg id is {}", msg_node->_recvNode->_msg_id);
                    auto call_back_iter = _fun_callbacks.find(msg_node->_recvNode->_msg_id);
                    if (call_back_iter == _fun_callbacks.end()) {
                        LOG_ERROR("invaild recv_msg's _msg_id : {}", msg_node->_recvNode->_msg_id);
                        _msg_que.pop();
                        continue;
                    }
                    call_back_iter->second(msg_node->_session, msg_node->_recvNode->_msg_id,
                                           msg_node->_recvNode->_data);
                }
            }

            // 没有停服，说明队列中有数据
            auto msg_node = _msg_que.front();
            LOG_INFO("recv_msg id is {}", msg_node->_recvNode->_msg_id);
            auto call_back_iter = _fun_callbacks.find(msg_node->_recvNode->_msg_id);
            if (call_back_iter == _fun_callbacks.end()) {
                LOG_ERROR("invaild recv_msg's _msg_id : {}", msg_node->_recvNode->_msg_id);
                _msg_que.pop();
                continue;
            }
            call_back_iter->second(
                msg_node->_session, msg_node->_recvNode->_msg_id,
                std::string(msg_node->_recvNode->_data, msg_node->_recvNode->_cur_len));

            _msg_que.pop();
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}

void LogicSystem::ResgisterCallBack()
{
    // 登录
    _fun_callbacks.insert(std::make_pair(
        MSG_IDS::ID_CHAT_LOGIN, std::bind(&LogicSystem::LoginHandler, this, std::placeholders::_1,
                                          std::placeholders::_2, std::placeholders::_3)));

    // 搜索用户
    _fun_callbacks.insert(
        std::make_pair(MSG_IDS::ID_SEARCH_USER_REQ,
                       std::bind(&LogicSystem::SearchUserHandler, this, std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3)));

    // 添加好友 请求
    _fun_callbacks.insert(
        std::make_pair(MSG_IDS::ID_ADD_FRIEND_REQ,
                       std::bind(&LogicSystem::AddFriendApply, this, std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3)));

    // 同意 好友请求
    _fun_callbacks.insert(
        std::make_pair(MSG_IDS::ID_AUTH_FRIEND_REQ,
                       std::bind(&LogicSystem::AuthFriendApply, this, std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3)));
    
    // 发送文本消息
    _fun_callbacks.insert(
        std::make_pair(MSG_IDS::ID_TEXT_CHAT_MSG_REQ,
                       std::bind(&LogicSystem::DealChatTextMsg, this, std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3)));
}

bool LogicSystem::GetBaseInfo(int uid, std::shared_ptr<UserInfo> &user_info)
{
    std::string base_key = USER_BASE_INFO + std::to_string(uid);
    std::string baseInfo_value = "";
    // 默认 存入 UserInfo 通过 json格式存储
    bool is_su = RedisMgr::GetInstance()->Get(base_key, baseInfo_value);
    if (is_su) {
        Json::Value ori_json;
        Json::CharReaderBuilder builder;
        Json::String errs;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        bool is_parser =
            reader->parse(baseInfo_value.c_str(), baseInfo_value.c_str() + baseInfo_value.size(),
                          &ori_json, &errs);
        if (!is_parser) {
            LOG_ERROR("json parse error : {}", errs);
            return false;
        }

        user_info->uid = ori_json["uid"].asInt();
        user_info->name = ori_json["name"].asString();
        user_info->pwd = ori_json["pwd"].asString();
        user_info->email = ori_json["email"].asString();
        user_info->nick = ori_json["nick"].asString();
        user_info->desc = ori_json["desc"].asString();
        user_info->sex = ori_json["sex"].asInt();
        user_info->icon = ori_json["icon"].asString();
        LOG_INFO("user login uid:{}, name:{}, pwd:{}, email:{}, nick:{}, desc:{}, sex:{}, icon:{}",
                 user_info->uid, user_info->name, user_info->pwd, user_info->email, user_info->nick,
                 user_info->desc, user_info->sex, user_info->icon);

    } else //  Redis里没有，那就 Mysql 里找
    {
        std::shared_ptr<UserInfo> userinfo = nullptr;
        userinfo = MysqlMgr::GetInstance()->GetUser(uid);
        if (userinfo == nullptr) {
            return false;
        }
        user_info = userinfo;

        LOG_INFO("user login uid:{}, name:{}, pwd:{}, email:{}, nick:{}, desc:{}, sex:{}, icon:{}",
                 user_info->uid, user_info->name, user_info->pwd, user_info->email, user_info->nick,
                 user_info->desc, user_info->sex, user_info->icon);

        Json::Value redis_root;
        redis_root["uid"] = uid;
        redis_root["pwd"] = user_info->pwd;
        redis_root["name"] = user_info->name;
        redis_root["email"] = user_info->email;
        redis_root["nick"] = user_info->nick;
        redis_root["desc"] = user_info->desc;
        redis_root["sex"] = user_info->sex;
        redis_root["icon"] = user_info->icon;
        RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
    }
    return true;
}

void LogicSystem::GetUserByUid(std::string uid_str, Json::Value &rtvalue)
{
    rtvalue["error"] = ErrorCodes::Success;

    std::string base_key = USER_BASE_INFO + uid_str;
    std::string info_str = "";
    bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
    if (b_base) {
        Json::Value root;
        Json::CharReaderBuilder readerBuilder;
        std::string errs;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        bool parsingSuccessful =
            reader->parse(info_str.c_str(), info_str.c_str() + info_str.size(), &root, &errs);
        if (!parsingSuccessful) {
            std::cerr << "JSON 解析失败: " << errs << std::endl;
            return;
        }
        // Json::Reader reader;
        // Json::Value root;
        // reader.parse(info_str, root);
        auto uid = root["uid"].asInt();
        auto name = root["name"].asString();
        auto pwd = root["pwd"].asString();
        auto email = root["email"].asString();
        auto nick = root["nick"].asString();
        auto desc = root["desc"].asString();
        auto sex = root["sex"].asInt();
        auto icon = root["icon"].asString();

        rtvalue["uid"] = uid;
        // rtvalue["pwd"] = pwd;
        rtvalue["name"] = name;
        rtvalue["email"] = email;
        rtvalue["nick"] = nick;
        rtvalue["desc"] = desc;
        rtvalue["sex"] = sex;
        rtvalue["icon"] = icon;
        return;
    }

    auto uid = std::stoi(uid_str);
    std::shared_ptr<UserInfo> user_info = nullptr;
    user_info = MysqlMgr::GetInstance()->GetUser(uid);
    if (user_info == nullptr) {
        rtvalue["error"] = ErrorCodes::UidInvalid;
        return;
    }

    Json::Value redis_root;
    redis_root["uid"] = user_info->uid;
    // redis_root["pwd"] = user_info->pwd;
    redis_root["name"] = user_info->name;
    redis_root["email"] = user_info->email;
    redis_root["nick"] = user_info->nick;
    redis_root["desc"] = user_info->desc;
    redis_root["sex"] = user_info->sex;
    redis_root["icon"] = user_info->icon;

    RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

    rtvalue["uid"] = user_info->uid;
    // rtvalue["pwd"] = user_info->pwd;
    rtvalue["name"] = user_info->name;
    rtvalue["email"] = user_info->email;
    rtvalue["nick"] = user_info->nick;
    rtvalue["desc"] = user_info->desc;
    rtvalue["sex"] = user_info->sex;
    rtvalue["icon"] = user_info->icon;
}

void LogicSystem::GetUserByName(std::string name, Json::Value &rtvalue)
{
    rtvalue["error"] = ErrorCodes::Success;
    std::string base_key = NAME_INFO + name;
    std::string info_str = "";
    bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
    if (b_base) {
        Json::Value root;
        Json::CharReaderBuilder readerBuilder;
        std::string errs;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        bool parsingSuccessful =
            reader->parse(info_str.c_str(), info_str.c_str() + info_str.size(), &root, &errs);
        if (!parsingSuccessful) {
            std::cerr << "JSON 解析失败: " << errs << std::endl;
            return;
        }
        auto uid = root["uid"].asInt();
        auto name = root["name"].asString();
        auto pwd = root["pwd"].asString();
        auto email = root["email"].asString();
        auto nick = root["nick"].asString();
        auto desc = root["desc"].asString();
        auto sex = root["sex"].asInt();
        auto icon = root["icon"].asString();

        rtvalue["uid"] = uid;
        // rtvalue["pwd"] = pwd;
        rtvalue["name"] = name;
        rtvalue["email"] = email;
        rtvalue["nick"] = nick;
        rtvalue["desc"] = desc;
        rtvalue["sex"] = sex;
        rtvalue["icon"] = icon;
        return;
    }

    std::shared_ptr<UserInfo> user_info = nullptr;
    user_info = MysqlMgr::GetInstance()->GetUser(name);
    if (user_info == nullptr) {
        rtvalue["error"] = ErrorCodes::UidInvalid;
        return;
    }

    Json::Value redis_root;
    redis_root["uid"] = user_info->uid;
    redis_root["pwd"] = user_info->pwd;
    redis_root["name"] = user_info->name;
    redis_root["email"] = user_info->email;
    redis_root["nick"] = user_info->nick;
    redis_root["desc"] = user_info->desc;
    redis_root["sex"] = user_info->sex;
    redis_root["icon"] = user_info->icon;

    RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
    rtvalue["uid"] = user_info->uid;
    // rtvalue["pwd"] = user_info->pwd;
    rtvalue["name"] = user_info->name;
    rtvalue["email"] = user_info->email;
    rtvalue["nick"] = user_info->nick;
    rtvalue["desc"] = user_info->desc;
    rtvalue["sex"] = user_info->sex;
    rtvalue["icon"] = user_info->icon;
}

bool LogicSystem::GetFriendApplyInfo(int uid, std::vector<std::shared_ptr<ApplyInfo>> &apply_list)
{
    return MysqlMgr::GetInstance()->GetApplyList(uid, apply_list, 0, 10);
}

bool LogicSystem::GetFriendList(int uid, std::vector<std::shared_ptr<UserInfo>> &friend_list)
{
    return MysqlMgr::GetInstance()->GetFriendList(uid, friend_list);
}

void LogicSystem::LoginHandler(std::shared_ptr<CSession> csession, const short &msg_id,
                               const std::string &msg_data)
{
    // 解析 MSG_IDS::ID_CHAT_LOGIN，客户端发送来的 LOGIN TCP请求
    Json::Value json;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value resp_json;
    Json::String error;
    // 最后保证，发送消息
    Defer defer([this, csession, &resp_json]() {
        csession->Send(resp_json.toStyledString(), MSG_IDS::ID_CHAT_LOGIN_RSP);
    });

    auto b_res = reader->parse(msg_data.c_str(), msg_data.c_str() + msg_data.size(), &json, &error);
    if (!b_res) {
        LOG_ERROR("json parse error");
        resp_json["error"] = ErrorCodes::Error_Json;
        return;
    }

    auto uid = json["uid"].asInt();
    auto token = json["token"].asString();
    LOG_INFO("user login uid is {}, user token is {}", uid, token);

    // 从状态服务器获取 token 匹配是否准确
    // auto rsp = StatusGrpcClient::GetInstance()->Login(uid, token);
    // 这里就直接查 Redis
    std::string uid_str = std::to_string(uid);
    std::string token_key = USERTOKENPREFIX + uid_str;
    std::string token_value = "";
    bool sucess = RedisMgr::GetInstance()->Get(token_key, token_value);
    if (!sucess) {
        resp_json["error"] = ErrorCodes::UidInvalid;
        return;
    }
    if (token != token_value) {
        resp_json["error"] = ErrorCodes::TokenInvalid;
        return;
    }

    // 此时，token认证成功
    // 查询 用户的基本信息
    auto user_info = std::make_shared<UserInfo>();
    bool b_base = GetBaseInfo(uid, user_info);
    if (!b_base) {
        resp_json["error"] = ErrorCodes::UidInvalid;
        return;
    }

    resp_json["uid"] = uid;
    resp_json["pwd"] = user_info->pwd;
    resp_json["name"] = user_info->name;
    resp_json["email"] = user_info->email;
    resp_json["nick"] = user_info->nick;
    resp_json["desc"] = user_info->desc;
    resp_json["sex"] = user_info->sex;
    resp_json["icon"] = user_info->icon;

    // 获取 好友申请 列表
    std::vector<std::shared_ptr<ApplyInfo>> apply_info;
    auto b_apply = GetFriendApplyInfo(uid, apply_info);
    if (b_apply) {
        for (auto &apply : apply_info) {
            Json::Value obj;
            obj["name"] = apply->_name;
            obj["uid"] = apply->_uid;
            obj["icon"] = apply->_icon;
            obj["nick"] = apply->_nick;
            obj["sex"] = apply->_sex;
            obj["desc"] = apply->_desc;
            obj["status"] = apply->_status;
            resp_json["apply_list"].append(obj);
        }
    }

    // 获取 好友 列表
    std::vector<std::shared_ptr<UserInfo>> friend_list;
    bool b_friend_list = GetFriendList(uid, friend_list);
    if (b_friend_list) {
        for (auto &friend_ele : friend_list) {
            Json::Value obj;
            obj["name"] = friend_ele->name;
            obj["uid"] = friend_ele->uid;
            obj["icon"] = friend_ele->icon;
            obj["nick"] = friend_ele->nick;
            obj["sex"] = friend_ele->sex;
            obj["desc"] = friend_ele->desc;
            resp_json["friend_list"].append(obj);
        }
    }

    // 自增 当前服务器 已连接 客户端数量。CSession
    auto server_name = ConfigMgr::GetInstance()["SelfServer"]["name"];
    int cur_count = 0;
    std::string count = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server_name);
    if (!count.empty()) {
        cur_count = std::stoi(count);
    }

    RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, std::to_string(cur_count + 1));

    // 当前CSession绑定 客户端uid
    csession->SetUserId(uid);

    // Redis里记录，用户登录所在的ChatServer
    std::string ipkey = USERIPPREFIX + uid_str;
    RedisMgr::GetInstance()->Set(ipkey, server_name);

    // uid和session绑定管理，方便后续的踢人操作
    UserMgr::GetInstance()->SetUserSession(uid, csession);

    resp_json["error"] = ErrorCodes::Success;
}

void LogicSystem::SearchUserHandler(std::shared_ptr<CSession> cession, const short &msg_id,
                                    const std::string &msg_data)
{
    Json::Value root;
    Json::Value resp_json;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::String error;

    Defer defer([&resp_json, &cession]() {
        cession->Send(resp_json.toStyledString(), ID_SEARCH_USER_RSP);
    });

    bool b_res = reader->parse(msg_data.c_str(), msg_data.c_str() + msg_data.size(), &root, &error);
    if (!b_res) {
        LOG_ERROR("json parse error");
        resp_json["error"] = ErrorCodes::Error_Json;
        return;
    }

    auto uid_str = root["uid"].asString();
    std::cout << "user SearchInfo uid is  " << uid_str << std::endl;

    // 判断 uuid 是 用户uid 还是 用户name
    if (isPureDigit(uid_str)) {
        GetUserByUid(uid_str, resp_json);
    } else {
        GetUserByName(uid_str, resp_json);
    }

    // 确保不把 pwd 隐私信息发送
    resp_json["pwd"] = "";
}

void LogicSystem::AddFriendApply(std::shared_ptr<CSession> cession, const short &msg_id,
                                 const std::string &msg_data)
{
    Json::Value root;
    Json::Value resp_json;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::String error;

    Defer defer(
        [&resp_json, &cession]() { cession->Send(resp_json.toStyledString(), ID_ADD_FRIEND_RSP); });

    bool b_res = reader->parse(msg_data.c_str(), msg_data.c_str() + msg_data.size(), &root, &error);
    if (!b_res) {
        LOG_ERROR("json parse error");
        resp_json["error"] = ErrorCodes::Error_Json;
        return;
    }
    auto uid = root["uid"].asInt();
    auto touid = root["touid"].asInt();
    LOG_INFO("Add Friend uid:{}, touid:{}", uid, touid);
    //1. 好友申请记录写入 Mysql
    MysqlMgr::GetInstance()->AddFriendApply(uid, touid);

    //2. Redis 搜索 对方所在 服务器，找到说明对方还没下线
    std::string to_ip_key = USERIPPREFIX + std::to_string(touid);
    std::string targetServer = "";
    auto b_ip = RedisMgr::GetInstance()->Get(to_ip_key, targetServer);
    if (!b_ip) {
        return;
    }

    auto apply_info = std::make_shared<UserInfo>();
    bool b_info = GetBaseInfo(uid, apply_info);

    auto &cfg = ConfigMgr::GetInstance();
    auto selfServerName = cfg["SelfServer"]["name"];
    LOG_INFO("targetServer:{}, selfServerName:{}", targetServer, selfServerName);
    //3. 同一服务器，找对方所在的CSession，通知对方的客户端
    if (targetServer == selfServerName) {
        auto perr_cession = UserMgr::GetInstance()->GetSession(touid);
        if (perr_cession == nullptr) {
            LOG_ERROR("perr cession is close, touid:{}", touid);
            return;
        }
        //在内存中则直接发送通知对方
        Json::Value notify;
        notify["error"] = ErrorCodes::Success;
        notify["applyuid"] = uid;
        notify["desc"] = "";
        if (b_info) {
            notify["applyname"] = apply_info->name;
            notify["icon"] = apply_info->icon;
            notify["sex"] = apply_info->sex;
            notify["nick"] = apply_info->nick;
        }
        perr_cession->Send(notify.toStyledString(), MSG_IDS::ID_NOTIFY_ADD_FRIEND_REQ);
    } else {
        //4. 不同服务器，gRPC通知 对端服务器~
        AddFriendReq add_req;
        add_req.set_applyuid(uid);
        add_req.set_touid(touid);
        add_req.set_desc("");
        if (b_info) {
            add_req.set_applyname(apply_info->name);
            add_req.set_icon(apply_info->icon);
            add_req.set_sex(apply_info->sex);
            add_req.set_nick(apply_info->nick);
        }

        ChatGrpcClient::GetInstance()->NotifyAddFriend(targetServer, add_req);
    }
}

void LogicSystem::AuthFriendApply(std::shared_ptr<CSession> cession, const short &msg_id,
                                  const std::string &msg_data)
{
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::String error;
    Json::Value resp_json;

    Defer defer([&resp_json, &cession]() {
        cession->Send(resp_json.toStyledString(), ID_AUTH_FRIEND_RSP);
    });

    bool b_res = reader->parse(msg_data.c_str(), msg_data.c_str() + msg_data.size(), &root, &error);
    if (!b_res) {
        LOG_ERROR("json parse error");
        resp_json["error"] = ErrorCodes::Error_Json;
        return;
    }

    // 被申请方 ，同意 申请方 的好友请求 ~
    auto fromuid = root["fromuid"].asInt(); // 申请方
    auto touid = root["touid"].asInt();     // 被申请方
    LOG_INFO("add frient, from:{}, to(auth):{},", fromuid, touid);

    resp_json["uid"] = fromuid;
    resp_json["error"] = ErrorCodes::Success;

    auto user_info = std::make_shared<UserInfo>();

    // 把 申请方的 基础信息 返回
    bool b_info = GetBaseInfo(fromuid, user_info);
    if (b_info) {
        resp_json["name"] = user_info->name;
        resp_json["nick"] = user_info->nick;
        resp_json["icon"] = user_info->icon;
        resp_json["sex"] = user_info->sex;
    } else {
        resp_json["error"] = ErrorCodes::UidInvalid;
    }
    LOG_INFO("AuthFriendApply，fromuid:{}, touid:{}", fromuid, touid);
    // 1. 写入数据库，把 friend_apply 修改
    MysqlMgr::GetInstance()->AuthFriendApply(fromuid, touid);

    // 2. 写入数据库，把好友关系添加到 friend
    MysqlMgr::GetInstance()->AddFriend(fromuid, touid);

    // 3. 通知对端服务器，
    // redis 找到对端 申请方 fromuid 的 ChatServer name
    auto from_ip_key = USERIPPREFIX + std::to_string(fromuid);
    std::string from_ip_value = "";
    bool b_ip = RedisMgr::GetInstance()->Get(from_ip_key, from_ip_value);
    if (!b_ip) {
        // 可能对方已经下线 ~
        return;
    }

    if (from_ip_value == ConfigMgr::GetInstance()["SelfServer"]["name"]) {
        // 在同一个ChatServer
        auto session = UserMgr::GetInstance()->GetSession(fromuid);
        if (session == nullptr) {
            // 可能对方已经下线 ~
            return;
        }
        Json::Value notify;
        notify["error"] = ErrorCodes::Success;
        // notify["fromuid"] = fromuid;
        notify["touid"] = touid;
        auto user_info = std::make_shared<UserInfo>();
        bool b_info = GetBaseInfo(touid, user_info);
        if (b_info) {
            notify["name"] = user_info->name;
            notify["nick"] = user_info->nick;
            notify["icon"] = user_info->icon;
            notify["sex"] = user_info->sex;
        } else {
            notify["error"] = ErrorCodes::UidInvalid;
        }
        session->Send(notify.toStyledString(), ID_NOTIFY_AUTH_FRIEND_REQ);

    } else {
        // 在不同的ChatServer，发送 NotifyAuthFriend rpc请求。
        AuthFriendReq req;
        req.set_fromuid(fromuid);
        req.set_touid(touid);

        ChatGrpcClient::GetInstance()->NotifyAuthFriend(from_ip_value, req);
    }
}

void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short &msg_id,
                                  const std::string &msg_data)
{
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
    bool parsingSuccessful =
        reader->parse(msg_data.c_str(), msg_data.c_str() + msg_data.size(), &root, &errs);
    if (!parsingSuccessful) {
        std::cerr << "JSON 解析失败: " << errs << std::endl;
        return;
    }

    auto uid = root["fromuid"].asInt();
    auto touid = root["touid"].asInt();

    const Json::Value arrays = root["text_array"];

    Json::Value rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    rtvalue["text_array"] = arrays;
    rtvalue["fromuid"] = uid;
    rtvalue["touid"] = touid;

    Defer defer([this, &rtvalue, session]() {
        std::string return_str = rtvalue.toStyledString();
        session->Send(return_str, ID_TEXT_CHAT_MSG_RSP);
    });

    // 查询redis 查找touid对应的server ip
    auto to_str = std::to_string(touid);
    auto to_ip_key = USERIPPREFIX + to_str;
    std::string to_ip_value = "";
    bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
    if (!b_ip) {
        return;
    }

    auto &cfg = ConfigMgr::GetInstance();
    auto self_name = cfg["SelfServer"]["name"];

    // 二者在同一个服务器上，直接通知对方有消息
    if (to_ip_value == self_name) {
        auto session = UserMgr::GetInstance()->GetSession(touid);
        if (session) {
            // 则直接发送通知对方
            std::string return_str = rtvalue.toStyledString();
            session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
        }
    } else {
        TextChatMsgReq text_msg_req;
        text_msg_req.set_fromuid(uid);
        text_msg_req.set_touid(touid);
        for (const auto &txt_obj : arrays) {
            auto content = txt_obj["content"].asString();
            auto msgid = txt_obj["msgid"].asString();
            std::cout << "content is " << content << std::endl;
            std::cout << "msgid is " << msgid << std::endl;
            auto *text_msg = text_msg_req.add_textmsgs();
            text_msg->set_msgid(msgid);
            text_msg->set_msgcontent(content);
        }

        // 发送通知
        ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
    }
}

} // namespace core