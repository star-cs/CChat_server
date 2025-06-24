/*
 * @Author: star-cs
 * @Date: 2025-06-11 16:03:31
 * @LastEditTime: 2025-06-24 14:21:28
 * @FilePath: /CChat_server/ChatServer/src/mysql_mgr.cc
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

std::shared_ptr<UserInfo> MysqlMgr::GetUser(int uid)
{
    return _dao.GetUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::GetUser(const std::string &name)
{
    return _dao.GetUser(name);
}

bool MysqlMgr::AddFriendApply(int uid, int touid)
{
    return _dao.AddFriendApply(uid, touid);
}

bool MysqlMgr::AuthFriendApply(int fromuid, int touid)
{
    return _dao.AuthFriendApply(fromuid, touid);
}
bool MysqlMgr::AddFriend(int fromuid, int touid)
{
    return _dao.AddFriend(fromuid, touid);
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

} // namespace core