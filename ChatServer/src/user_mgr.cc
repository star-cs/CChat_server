/*
 * @Author: star-cs
 * @Date: 2025-06-21 21:01:55
 * @LastEditTime: 2025-06-22 11:10:34
 * @FilePath: /CChat_server/ChatServer/src/user_mgr.cc
 * @Description:
 */

#include "user_mgr.h"

namespace core
{
    UserMgr::UserMgr()
    {
    }

    UserMgr::~UserMgr()
    {
        _uid_to_session.clear();
    }

    std::shared_ptr<CSession> UserMgr::GetSession(int uid)
    {
        std::unique_lock<std::mutex> lock(_session_mtx);
        auto it = _uid_to_session.find(uid);
        if (it == _uid_to_session.end())
        {
            return nullptr;
        }
        return it->second;
    }

    void UserMgr::SetUserSession(int uid, std::shared_ptr<CSession> session)
    {
        std::unique_lock<std::mutex> lock(_session_mtx);
        _uid_to_session[uid] = session;
    }

    void UserMgr::RmvUserSession(int uid)
    {
        auto uid_str = std::to_string(uid);
        {
            std::lock_guard<std::mutex> lock(_session_mtx);
            _uid_to_session.erase(uid);
        }
    }
}
