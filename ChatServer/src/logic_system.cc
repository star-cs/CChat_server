/*
 * @Author: star-cs
 * @Date: 2025-06-16 10:00:05
 * @LastEditTime: 2025-07-08 21:27:45
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
LogicSystem::LogicSystem() : _b_stop(false), _p_server(nullptr)
{
    ResgisterCallBack();
    _worker_thread = std::thread(&LogicSystem::DelMsg, this);
}

LogicSystem::~LogicSystem()
{
    std::cout << "~LogicSystem destruct" << std::endl;
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
                    LOG_INFO("receive reqid is {}, data is {}", msg_node->_recvNode->_msg_id,
                             msg_node->_recvNode->_data);

                    call_back_iter->second(msg_node->_session, msg_node->_recvNode->_msg_id,
                                           msg_node->_recvNode->_data);
                }
                break;
            }

            // 没有停服，说明队列中有数据
            auto msg_node = _msg_que.front();
            auto call_back_iter = _fun_callbacks.find(msg_node->_recvNode->_msg_id);
            if (call_back_iter == _fun_callbacks.end()) {
                LOG_ERROR("invaild recv_msg's reqid is {}, data is {}",
                          msg_node->_recvNode->_msg_id, msg_node->_recvNode->_data);
                _msg_que.pop();
                continue;
            }
            LOG_INFO("receive recv_msg's reqid is {}, data is {}", msg_node->_recvNode->_msg_id,
                     msg_node->_recvNode->_data);

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

    // 处理 客户端发来的 心跳包
    _fun_callbacks.insert(
        std::make_pair(MSG_IDS::ID_HEART_BEAT_REQ,
                       std::bind(&LogicSystem::HeartBeatHandler, this, std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3)));

    // ID_LOAD_CHAT_THREAD_REQ 处理加载聊天会话请求
    _fun_callbacks.insert(
        std::make_pair(MSG_IDS::ID_LOAD_CHAT_THREAD_REQ,
                       std::bind(&LogicSystem::GetUserThreadsHandler, this, std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3)));

    // ID_CREATE_PRIVATE_CHAT_REQ 创建新的聊天会话
    _fun_callbacks.insert(
        std::make_pair(MSG_IDS::ID_CREATE_PRIVATE_CHAT_REQ,
                       std::bind(&LogicSystem::CreatePrivateChat, this, std::placeholders::_1,
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
        LOG_INFO("user info uid:{}, name:{}, pwd:{}, email:{}, nick:{}, desc:{}, sex:{}, icon:{}",
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

        LOG_INFO("user info uid:{}, name:{}, pwd:{}, email:{}, nick:{}, desc:{}, sex:{}, icon:{}",
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

bool LogicSystem::GetUserThreads(int64_t userId, int64_t lastId, int pageSize,
                                 std::vector<std::shared_ptr<ChatThreadInfo>> &threads,
                                 bool &loadMore, int &nextLastId)
{
    return MysqlMgr::GetInstance()->GetUserThreads(userId, lastId, pageSize, threads, loadMore,
                                                   nextLastId);
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

    auto self_name = ConfigMgr::GetInstance().GetSelfName();
    {
        // 此时，token认证成功
        // 此处，添加分布式锁，让该用户独占登录
        auto lock_key = LOCK_PREFIX + uid_str;
        auto identifier =
            RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
        //利用defer解锁
        Defer defer2([this, identifier, lock_key]() {
            RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
        });

        //此处判断该用户是否在别处或者本服务器登录
        std::string ipvalue = "";
        std::string ipkey = USERIPPREFIX + uid_str;
        bool b_ip = RedisMgr::GetInstance()->Get(ipkey, ipvalue);
        if (b_ip) {
            auto &cfg = ConfigMgr::GetInstance();
            if (self_name == ipvalue) {
                // 单服务器踢人
                auto old_session = UserMgr::GetInstance()->GetSession(uid);

                // 此次应该发送给客户端踢人消息，让客户端自己退出
                if (old_session) {
                    old_session->NotifyOffline(uid);
                    // 清除旧的连接
                    _p_server->ClearSession(old_session->GetSessionId());
                }
            } else {
                // 如果不是本服务器，则通知grpc通知其他服务器踢掉
                ChatGrpcClient::GetInstance()->NotifyKickUser(ipvalue, uid);
            }
        }
        // 没有登录过 ~

        RedisMgr::GetInstance()->IncreaseCount(self_name);

        // 当前CSession绑定 客户端uid
        csession->SetUserId(uid);

        // Redis里记录，用户登录所在的ChatServer
        RedisMgr::GetInstance()->Set(ipkey, self_name);

        std::string uid_session_key = USER_SESSION_PREFIX + uid_str;
        // uid 和 session uid 也保存到 Redis，比如 下线的时候，查找 Redis 判断是否 匹配（匹配就清除，不匹配说明session uid被新登录重写了）
        RedisMgr::GetInstance()->Set(uid_session_key, csession->GetSessionId());

        // uid和session绑定管理，方便后续的踢人操作
        UserMgr::GetInstance()->SetUserSession(uid, csession);
    }

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
    auto desc = root["call_str"].asString();
    auto bakname = root["bakname"].asString();
    auto touid = root["touid"].asInt();
    LOG_INFO("user login uid is  {}; applydesc  is  {}; bakname is {}; touid is {}", uid, desc,
             bakname, touid);

    //先更新数据库
    MysqlMgr::GetInstance()->AddFriendApply(uid, touid, desc, bakname);

    //查询redis 查找touid对应的server ip
    auto to_str = std::to_string(touid);
    auto to_ip_key = USERIPPREFIX + to_str;
    std::string to_ip_value = "";
    bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
    if (!b_ip) {
        return;
    }

    auto &cfg = ConfigMgr::GetInstance();
    auto self_name = cfg["SelfServer"]["Name"];

    auto apply_info = std::make_shared<UserInfo>();
    bool b_info = GetBaseInfo(uid, apply_info);

    //直接通知对方有申请消息
    if (to_ip_value == self_name) {
        auto session = UserMgr::GetInstance()->GetSession(touid);
        if (session) {
            //在内存中则直接发送通知对方
            Json::Value notify;
            notify["error"] = ErrorCodes::Success;
            notify["applyuid"] = uid;
            notify["name"] = apply_info->name;
            notify["desc"] = desc;
            if (b_info) {
                notify["icon"] = apply_info->icon;
                notify["sex"] = apply_info->sex;
                notify["nick"] = apply_info->nick;
            }
            std::string return_str = notify.toStyledString();
            session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
        }

        return;
    }

    AddFriendReq add_req;
    add_req.set_applyuid(uid);
    add_req.set_touid(touid);
    add_req.set_applyname(apply_info->name);
    add_req.set_desc(desc);
    if (b_info) {
        add_req.set_icon(apply_info->icon);
        add_req.set_sex(apply_info->sex);
        add_req.set_nick(apply_info->nick);
    }

    //发送通知
    ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value, add_req);
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

    auto fromuid = root["fromuid"].asInt();
    auto touid = root["touid"].asInt();
    auto backname = root["backname"].asString();
    auto desc = root["call_str"].asString();
    LOG_INFO("add frient, from(auth):{}, to(apply):{}, backname:{}, desc:{}", fromuid, touid,
             backname, desc);

    resp_json["uid"] = touid;
    resp_json["error"] = ErrorCodes::Success;

    auto user_info = std::make_shared<UserInfo>();

    // 把 申请方的 基础信息 返回
    bool b_info = GetBaseInfo(touid, user_info);
    if (b_info) {
        resp_json["name"] = user_info->name;
        resp_json["nick"] = user_info->nick;
        resp_json["icon"] = user_info->icon;
        resp_json["sex"] = user_info->sex;
    } else {
        resp_json["error"] = ErrorCodes::UidInvalid;
    }

    // 把 打招呼的话和备注，写到数据库里。
    // friend_apply friend 可以一起事务处理 ~
    std::vector<std::shared_ptr<AddFriendmsg>> chat_datas;

    bool b_addFriend =
        MysqlMgr::GetInstance()->AddFriend(fromuid, touid, backname, desc, chat_datas);
    if (b_addFriend == false) {
        LOG_ERROR("AddFriend failed");
        return;
    }

    // 解决bug，之前存在对端不在线就没把消息回传给 好友同意方
    for (auto &chat_data : chat_datas) {
        Json::Value chat;
        chat["sender"] = chat_data->sender_id;
        chat["msg_id"] = chat_data->msg_id;
        chat["thread_id"] = chat_data->thread_id;
        chat["unique_id"] = chat_data->unique_id;
        chat["msg_content"] = chat_data->msgcontent;
        // 把消息再发回同意人
        resp_json["chat_datas"].append(chat);
    }

    // 3. 通知对端服务器，
    // redis 找到对端 申请方 fromuid 的 ChatServer name
    auto to_ip_key = USERIPPREFIX + std::to_string(touid);
    std::string to_ip_value = "";
    bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
    if (!b_ip) {
        // 可能对方已经下线 ~
        return;
    }

    if (to_ip_value == ConfigMgr::GetInstance().GetSelfName()) {
        // 在同一个ChatServer
        auto session = UserMgr::GetInstance()->GetSession(touid);
        if (session == nullptr) {
            // 可能对方已经下线 ~
            return;
        }
        Json::Value notify;
        notify["error"] = ErrorCodes::Success;
        // 同意方的基础信息
        notify["fromuid"] = fromuid;
        // notify["touid"] = touid;
        auto user_info = std::make_shared<UserInfo>();
        bool b_info = GetBaseInfo(fromuid, user_info);
        if (b_info) {
            notify["name"] = user_info->name;
            notify["nick"] = user_info->nick;
            notify["icon"] = user_info->icon;
            notify["sex"] = user_info->sex;
        } else {
            notify["error"] = ErrorCodes::UidInvalid;
        }

        for (auto &chat_data : chat_datas) {
            Json::Value chat;
            chat["sender"] = chat_data->sender_id;
            chat["msg_id"] = chat_data->msg_id;
            chat["thread_id"] = chat_data->thread_id;
            chat["unique_id"] = chat_data->unique_id;
            chat["msg_content"] = chat_data->msgcontent;
            notify["chat_datas"].append(chat);
        }

        session->Send(notify.toStyledString(), ID_NOTIFY_AUTH_FRIEND_REQ);

    } else {
        // 在不同的ChatServer，发送 NotifyAuthFriend rpc请求。
        AuthFriendReq auth_req;
        auth_req.set_fromuid(fromuid);
        auth_req.set_touid(touid);

        for (auto &chat_data : chat_datas) {
            auto text_msg = auth_req.add_textmsgs();
            text_msg->set_sender_id(chat_data->sender_id);
            text_msg->set_msg_id(chat_data->msg_id);
            text_msg->set_thread_id(chat_data->thread_id);
            text_msg->set_unique_id(chat_data->unique_id);
            text_msg->set_msgcontent(chat_data->msgcontent);
        }

        ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
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

    // todo 插入数据库
    //

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
    auto self_name = cfg.GetSelfName();

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
            auto unique_id = txt_obj["unique_id"].asString();
            auto *text_msg = text_msg_req.add_textmsgs();
            text_msg->set_unique_id(unique_id);
            text_msg->set_msgcontent(content);
        }

        // 发送通知
        ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
    }
}

void LogicSystem::HeartBeatHandler(std::shared_ptr<CSession> session, const short &msg_id,
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
    LOG_INFO("receive heart beat msg, uid is {}", uid);
    Json::Value rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    session->Send(rtvalue.toStyledString(), ID_HEART_BEAT_RSP);
}

void LogicSystem::GetUserThreadsHandler(std::shared_ptr<CSession> session, const short &msg_id,
                                        const std::string &msg_data)
{
    // 数据库加载 chat_threads记录
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

    auto uid = root["uid"].asInt();
    auto last_id = root["thread_id"].asInt();
    LOG_INFO("get uid:{} threads", uid);

    Json::Value rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    rtvalue["uid"] = uid;
    Defer defer([this, &rtvalue, session]() {
        std::string return_str = rtvalue.toStyledString();
        session->Send(return_str, ID_LOAD_CHAT_THREAD_RSP);
    });

    // 从数据库，联合查找 private_chat 和 group_chat_member
    std::vector<std::shared_ptr<ChatThreadInfo>> threads;
    int page_size = 10;
    bool load_more = false;
    int next_last_id = 0;
    bool res = GetUserThreads(uid, last_id, page_size, threads, load_more, next_last_id);
    if (!res) {
        rtvalue["error"] = ErrorCodes::UidInvalid;
        return;
    }

    rtvalue["load_more"] = load_more;
    rtvalue["next_last_id"] = (int)next_last_id;
    for (auto &thread : threads) {
        Json::Value thread_value;
        thread_value["thread_id"] = int(thread->_thread_id);
        thread_value["type"] = thread->_type;
        thread_value["user1_id"] = thread->_user1_id;
        thread_value["user2_id"] = thread->_user2_id;
        rtvalue["threads"].append(thread_value);
    }
}

void LogicSystem::CreatePrivateChat(std::shared_ptr<CSession> session, const short &msg_id,
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

    auto uid = root["uid"].asInt();
    auto other_id = root["other_id"].asInt();

    Json::Value rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    rtvalue["uid"] = uid;
    rtvalue["other_id"] = other_id;

    Defer defer([this, &rtvalue, session]() {
        std::string return_str = rtvalue.toStyledString();
        session->Send(return_str, ID_CREATE_PRIVATE_CHAT_RSP);
    });

    int thread_id = 0;
    bool res = MysqlMgr::GetInstance()->CreatePrivateChat(uid, other_id, thread_id);
    if (!res) {
        rtvalue["error"] = ErrorCodes::CREATE_CHAT_FAILED;
        return;
    }
    rtvalue["thread_id"] = thread_id;
}
} // namespace core