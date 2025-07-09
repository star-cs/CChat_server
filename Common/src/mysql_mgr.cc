/*
 * @Author: star-cs
 * @Date: 2025-06-30 11:24:36
 * @LastEditTime: 2025-07-08 22:33:29
 * @FilePath: /CChat_server/Common/src/mysql_mgr.cc
 * @Description: 
 */
#include "mysql_mgr.h"

namespace core
{
MysqlMgr::MysqlMgr()
{
}

MysqlMgr::~MysqlMgr()
{
}

int MysqlMgr::RegUser(const std::string &name, const std::string &email, const std::string &pwd,
                      const std::string &icon)
{
    return _dao.RegUserTransaction(name, email, pwd, icon);
}

bool MysqlMgr::CheckEmail(const std::string &name, const std::string &email)
{
    return _dao.CheckEmail(name, email);
}

bool MysqlMgr::UpdatePwd(const std::string &email, const std::string &pwd)
{
    return _dao.UpdatePwd(email, pwd);
}

bool MysqlMgr::CheckPwd(const std::string &email, const std::string &pwd, UserInfo &userInfo)
{
    return _dao.CheckPwd(email, pwd, userInfo);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(int uid)
{
    return _dao.GetUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(const std::string &name)
{
    return _dao.GetUser(name);
}

bool MysqlMgr::AddFriendApply(int uid, int touid, const std::string &desc,
                              const std::string &bakname)
{
    return _dao.AddFriendApply(uid, touid, desc, bakname);
}

bool MysqlMgr::AuthFriendApply(int fromuid, int touid)
{
    return _dao.AuthFriendApply(fromuid, touid);
}
bool MysqlMgr::AddFriend(int fromuid, int touid, const std::string &bakname,
                         const std::string &desc,
                         std::vector<std::shared_ptr<AddFriendmsg>> &chat_datas)
{
    return _dao.AddFriend(fromuid, touid, bakname, desc, chat_datas);
}

bool MysqlMgr::GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>> &applyList,
                            int begin, int limit)
{
    return _dao.GetApplyList(touid, applyList, begin, limit);
}
bool MysqlMgr::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> &user_info)
{
    return _dao.GetFriendList(self_id, user_info);
}

bool MysqlMgr::GetUserThreads(int64_t userId, int64_t lastId, int pageSize,
                              std::vector<std::shared_ptr<ChatThreadInfo>> &threads, bool &loadMore,
                              int &nextLastId)
{
    return _dao.GetUserThreads(userId, lastId, pageSize, threads, loadMore, nextLastId);
}

bool MysqlMgr::CreatePrivateChat(int user1_id, int user2_id, int &thread_id)
{
    return _dao.CreatePrivateChat(user1_id, user2_id, thread_id);
}

std::shared_ptr<PageResult> MysqlMgr::LoadChatMsg(int thread_id, int message_id, int page_size)
{
    return _dao.LoadChatMsg(thread_id, message_id, page_size);
}

} // namespace core