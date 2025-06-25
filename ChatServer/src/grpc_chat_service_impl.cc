/*
 * @Author: star-cs
 * @Date: 2025-06-20 21:40:08
 * @LastEditTime: 2025-06-25 14:43:39
 * @FilePath: /CChat_server/ChatServer/src/grpc_chat_service_impl.cc
 * @Description:
 */
#include "grpc_chat_service_impl.h"
#include "chat_service.pb.h"
#include "common.h"
#include "mysql_mgr.h"
#include "user_mgr.h"
#include <grpcpp/impl/codegen/status.h>
#include <json/value.h>
#include <json/reader.h>
#include <json/json.h>
#include "redis_mgr.h"

namespace core
{
ChatServiceImpl::ChatServiceImpl()
{
}
Status ChatServiceImpl::NotifyAddFriend(ServerContext *context, const AddFriendReq *request,
                                        AddFriendRsp *reply)
{
    // 处理 收到的好友申请连接

    // 1. 客户端没下线，tcp发给客户端
    auto touid = request->touid();
    LOG_INFO("receive NotifyAddFriend notify uid:{}", touid);

    Defer defer([request, reply]() {
        reply->set_error(ErrorCodes::Success);
        reply->set_applyuid(request->applyuid());
        reply->set_touid(request->touid());
    });

    auto session = UserMgr::GetInstance()->GetSession(touid);
    if (session == nullptr) {
        return Status::OK;
    }

    Json::Value rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    rtvalue["applyuid"] = request->applyuid();
    rtvalue["applyname"] = request->applyname();
    rtvalue["desc"] = request->desc();
    rtvalue["icon"] = request->icon();
    rtvalue["sex"] = request->sex();
    rtvalue["nick"] = request->nick();

    session->Send(rtvalue.toStyledString(), MSG_IDS::ID_NOTIFY_ADD_FRIEND_REQ);

    // 2. 下线，就不操作。~
    return Status::OK;
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext *context, const AuthFriendReq *request,
                                         AuthFriendRsp *response)
{
    // 通知 客户端，好友申请已经被 对方通过 ~
    auto fromuid = request->fromuid();
    auto touid = request->touid();
    LOG_INFO("ChatServiceImpl::NotifyAuthFriend fromuid:{}, touid(auth):{}", fromuid, touid);

    // 查询 fromuid 是否还在线
    auto session = UserMgr::GetInstance()->GetSession(fromuid);

    Defer defer([request, response]() {
        response->set_error(ErrorCodes::Success);
        response->set_fromuid(request->fromuid());
        response->set_touid(request->touid());
    });

    if (session == nullptr) {
        LOG_DEBUG("uid:{} session 不存在", fromuid);
        return Status::OK;
    }

    Json::Value rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    // rtvalue["fromuid"] = request->fromuid();
    rtvalue["touid"] = request->touid();

    // 获取到 touid 的 UserInfo，发送给 fromuid，作为 qt 结构体 AuthInfo 添加到 通讯录 内 ~
    auto touidInfo = std::make_shared<UserInfo>();
    bool b_info = GetBaseInfo(touid, touidInfo);
    if (b_info) {
        rtvalue["name"] = touidInfo->name;
        rtvalue["nick"] = touidInfo->nick;
        rtvalue["icon"] = touidInfo->icon;
        rtvalue["sex"] = touidInfo->sex;
    } else {
        rtvalue["error"] = ErrorCodes::UidInvalid;
    }
    LOG_DEBUG("{}", rtvalue.toStyledString());
    session->Send(rtvalue.toStyledString(), ID_NOTIFY_AUTH_FRIEND_REQ);
    return Status::OK;
}

Status ChatServiceImpl::NotifyTextChatMsg(::grpc::ServerContext *context,
                                          const TextChatMsgReq *request, TextChatMsgRsp *response)
{
    // 查找用户是否在本服务器
    auto touid = request->touid();
    auto session = UserMgr::GetInstance()->GetSession(touid);
    response->set_error(ErrorCodes::Success);
    if (session == nullptr) {
        return Status::OK;
    }

    Json::Value rtvalue;
    rtvalue["error"] = ErrorCodes::Success;
    rtvalue["fromuid"] = request->fromuid();
    rtvalue["touid"] = request->touid();

    // 组装连天数据
    Json::Value text_array;
    for (auto &msg : request->textmsgs()) {
        Json::Value element;
        element["content"] = msg.msgcontent();
        element["msgid"] = msg.msgid();
        text_array.append(element);
    }
    rtvalue["text_array"] = text_array;

    std::string return_str = rtvalue.toStyledString();

    session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
    return Status::OK;
}

bool ChatServiceImpl::GetBaseInfo(int uid, std::shared_ptr<UserInfo> &userinfo)
{
    std::string base_key = USER_BASE_INFO + std::to_string(uid);
    std::string info_str = "";
    auto b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
    if (b_base) {
        Json::Value root;
        Json::CharReaderBuilder readerBuilder;
        std::string errs;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
        bool parsingSuccessful =
            reader->parse(info_str.c_str(), info_str.c_str() + info_str.size(), &root, &errs);
        if (!parsingSuccessful) {
            std::cerr << "JSON 解析失败: " << errs << std::endl;
            return false;
        }

        userinfo->uid = root["uid"].asInt();
        userinfo->name = root["name"].asString();
        userinfo->pwd = root["pwd"].asString();
        userinfo->email = root["email"].asString();
        userinfo->nick = root["nick"].asString();
        userinfo->desc = root["desc"].asString();
        userinfo->sex = root["sex"].asInt();
        userinfo->icon = root["icon"].asString();
        std::cout << "user login uid is  " << userinfo->uid << " name  is " << userinfo->name
                  << " pwd is " << userinfo->pwd << " email is " << userinfo->email << std::endl;
    } else {
        std::shared_ptr<UserInfo> user_info = nullptr;
        user_info = MysqlMgr::GetInstance()->GetUser(uid);
        if (user_info == nullptr) {
            return false;
        }

        userinfo = user_info;

        Json::Value redis_root;
        redis_root["uid"] = uid;
        redis_root["pwd"] = userinfo->pwd;
        redis_root["name"] = userinfo->name;
        redis_root["email"] = userinfo->email;
        redis_root["nick"] = userinfo->nick;
        redis_root["desc"] = userinfo->desc;
        redis_root["sex"] = userinfo->sex;
        redis_root["icon"] = userinfo->icon;
        RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
    }

    return true;
}

} // namespace core