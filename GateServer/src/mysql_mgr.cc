/*
 * @Author: star-cs
 * @Date: 2025-06-11 16:03:31
 * @LastEditTime: 2025-06-13 15:40:10
 * @FilePath: /CChat_server/GateServer/src/mysql_mgr.cc
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

    bool MysqlMgr::TestProcedure(const std::string &email, int &uid, const std::string &name)
    {
        return _dao.TestProcedure(email, uid, name);
    }
}