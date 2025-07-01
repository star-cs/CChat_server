#pragma once
#include "mysql_dao.h"
#include "singleton.h"
namespace core
{
class MysqlMgr : public Singleton<MysqlMgr>
{
    friend class Singleton<MysqlMgr>;

public:
    ~MysqlMgr();
    int RegUser(const std::string &name, const std::string &email, const std::string &pwd, const std::string &icon);
    bool CheckEmail(const std::string &name, const std::string &email);
    bool UpdatePwd(const std::string &email, const std::string &pwd);
    bool CheckPwd(const std::string &email, const std::string &pwd, UserInfo &userInfo);

    std::shared_ptr<UserInfo> GetUser(int uid);
    std::shared_ptr<UserInfo> GetUser(const std::string &name);
    bool AddFriendApply(int uid, int touid);

    // 修改 friend_apply，fromuid 为申请方，touid为被申请方
    bool AuthFriendApply(int fromuid, int touid);
    // 添加好友记录 ~
    bool AddFriend(int fromuid, int touid);

    // 获取 touid 接收到的所有 好友申请
    bool GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>> &applyList, int begin,
                      int limit = 10);
                      
    // 获取 self_id 所有的 好友信息
    bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> &user_info);

private:
    MysqlMgr();
    MysqlDao _dao;
};

} // namespace core
