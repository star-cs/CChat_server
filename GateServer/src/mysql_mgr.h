/*
 * @Author: star-cs
 * @Date: 2025-06-11 16:03:25
 * @LastEditTime: 2025-06-13 19:12:29
 * @FilePath: /CChat_server/GateServer/src/mysql_mgr.h
 * @Description:
 */
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
        bool TestProcedure(const std::string &email, int &uid, const std::string &name);

    private:
        MysqlMgr();
        MysqlDao _dao;
    };

} // namespace core
