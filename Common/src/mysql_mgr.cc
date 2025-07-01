#include "mysql_mgr.h"

namespace core
{
MysqlMgr::MysqlMgr()
{
}

MysqlMgr::~MysqlMgr()
{
}

int MysqlMgr::RegUser(const std::string &name, const std::string &email, const std::string &pwd, const std::string &icon)
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