/*
 * @Author: star-cs
 * @Date: 2025-06-16 09:51:32
 * @LastEditTime: 2025-07-08 21:56:42
 * @FilePath: /CChat_server/ChatServer/src/logic_system.h
 * @Description: 
 */
#pragma once
#include "common.h"
#include "cserver.h"
#include "csession.h"
#include "singleton.h"
#include "mysql_mgr.h"
#include <json/reader.h>
#include <json/value.h>
#include <json/json.h>
#include <memory>
#include <vector>

namespace core
{
typedef std::function<void(std::shared_ptr<CSession>, const short &msg_id,
                           const std::string &msg_data)>
    FunCallBack;
class LogicSystem : public Singleton<LogicSystem>
{
    friend Singleton<LogicSystem>;

public:
    ~LogicSystem();
    void setCServer(std::shared_ptr<CServer> pCServer) { _p_server = pCServer; }
    void PostMsgToQue(std::shared_ptr<LogicNode> msg);

private:
    LogicSystem();
    void DelMsg();
    void ResgisterCallBack();

    /**
         * 1. 通过 USER_BASE_INFO + uid 从 Redis 获取用户基本信息
         * 2. redis没有就，通过uid从 Mysql获取用户基本信息，再同步到 Redis里。
         */
    bool GetBaseInfo(int uid, std::shared_ptr<UserInfo> &user_info);

    void GetUserByUid(std::string uid_str, Json::Value &rtvalue);
    void GetUserByName(std::string name, Json::Value &rtvalue);

    bool GetFriendApplyInfo(int uid, std::vector<std::shared_ptr<ApplyInfo>> &apply_list);
    bool GetFriendList(int uid, std::vector<std::shared_ptr<UserInfo>> &friend_list);

    bool GetUserThreads(int64_t userId, int64_t lastId, int pageSize,
                        std::vector<std::shared_ptr<ChatThreadInfo>> &threads, bool &loadMore,
                        int &nextLastId);

    // 处理 ID_CHAT_LOGIN 消息
    void LoginHandler(std::shared_ptr<CSession> cession, const short &msg_id,
                      const std::string &msg_data);

    // 处理 ID_SEARCH_USER_REQ
    void SearchUserHandler(std::shared_ptr<CSession> cession, const short &msg_id,
                           const std::string &msg_data);

    // 处理 ID_ADD_FRIEND_RSP 发出 好友请求 的 请求
    void AddFriendApply(std::shared_ptr<CSession> cession, const short &msg_id,
                        const std::string &msg_data);

    // 处理 ID_AUTH_FRIEND_REQ 同意 好友请求 的 请求
    void AuthFriendApply(std::shared_ptr<CSession> cession, const short &msg_id,
                         const std::string &msg_data);

    // 处理 ID_TEXT_CHAT_MSG_REQ 发送文本消息的 请求
    void DealChatTextMsg(std::shared_ptr<CSession> session, const short &msg_id,
                         const std::string &msg_data);

    // 处理客户端发来的心跳包
    void HeartBeatHandler(std::shared_ptr<CSession> session, const short &msg_id,
                          const std::string &msg_data);

    void GetUserThreadsHandler(std::shared_ptr<CSession> session, const short &msg_id,
                               const std::string &msg_data);

    void CreatePrivateChat(std::shared_ptr<CSession> session, const short &msg_id,
                           const std::string &msg_data);

    void LoadChatMsg(std::shared_ptr<CSession> session, const short &msg_id,
                     const std::string &msg_data);

private:
    std::thread _worker_thread;
    std::queue<std::shared_ptr<LogicNode>> _msg_que;
    std::mutex _mutex;
    std::condition_variable _cond;
    bool _b_stop;
    std::shared_ptr<CServer> _p_server;
    std::map<short, FunCallBack> _fun_callbacks;
};
} // namespace core
