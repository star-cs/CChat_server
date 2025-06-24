/*
 * @Author: star-cs
 * @Date: 2025-06-21 21:01:20
 * @LastEditTime: 2025-06-21 21:08:43
 * @FilePath: /CChat_server/ChatServer/src/user_mgr.h
 * @Description: 记录着当前 ChatServer 所有的 uid - CSession 的对应关系
 */
#pragma once
#include "csession.h"
#include "singleton.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace core
{
    class CSession;
    class UserMgr : public Singleton<UserMgr>
    {
        friend class Singleton<UserMgr>;

    public:
        ~UserMgr();
        std::shared_ptr<CSession> GetSession(int uid);
        void SetUserSession(int uid, std::shared_ptr<CSession> session);
        void RmvUserSession(int uid);

    private:
        UserMgr();
        std::mutex _session_mtx;
        std::unordered_map<int, std::shared_ptr<CSession>> _uid_to_session;
    };
}