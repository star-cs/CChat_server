/*
 * @Author: star-cs
 * @Date: 2025-06-11 11:22:37
 * @LastEditTime: 2025-07-08 22:34:56
 * @FilePath: /CChat_server/Common/src/mysql_dao.h
 * @Description: 数据 DAO 层
 * 
 * sql::Statement           普通的 SQL 语句执行接口
 * sql::PreparedStatement   预处理语句，带 ？ 占位符    [多次执行/动态参数/安全性高场景]
 * 
 * sql::ResultSet           SELECT查询后结果
 * 
 */
#pragma once
#include <mysql_connection.h>
#include <mysql_driver.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "common.h"

namespace core
{
class SqlConnection
{
public:
    SqlConnection(sql::Connection *con, int64_t lasttime) : _conn(con), _last_oper_time(lasttime) {}
    std::unique_ptr<sql::Connection> _conn;
    int64_t _last_oper_time;
};

class MySqlPool
{
public:
    MySqlPool(const std::string &url, const std::string &user, const std::string &pass,
              const std::string &schema, int poolsize);

    /**
         * 独立线程每 60s 执行
         * 连接 保活
         */
    void checkConnection();
    bool reconnect(long long timestamp);

    std::unique_ptr<SqlConnection> getConnection();

    void returnConnection(std::unique_ptr<SqlConnection> con);

    void Close();

    ~MySqlPool();

private:
    std::string _url;
    std::string _user;
    std::string _pass;
    std::string _schema;
    int _poolsize;
    std::queue<std::unique_ptr<SqlConnection>> _conns;
    std::mutex _mutex;
    std::condition_variable _cond;
    std::atomic<bool> _b_stop;
    std::thread _check_thread;
    std::atomic<int> _fail_count;
};

class MysqlDao
{
public:
    MysqlDao();
    ~MysqlDao();

    // GateServer里的 数据库操作
    // // 存储过程
    int RegUser(const std::string &name, const std::string &email, const std::string &pwd);

    // // 分离出每一步查询操作
    int RegUserTransaction(const std::string &name, const std::string &email,
                           const std::string &pwd, const std::string &icon);
    bool CheckEmail(const std::string &name, const std::string &email);
    bool UpdatePwd(const std::string &email, const std::string &newpwd);
    bool CheckPwd(const std::string &email, const std::string &pwd, UserInfo &userInfo);

    std::shared_ptr<UserInfo> GetUser(int uid);
    std::shared_ptr<UserInfo> GetUser(const std::string &name);
    bool AddFriendApply(int uid, int touid, const std::string &desc, const std::string &bakname);
    bool AuthFriendApply(int fromuid, int touid);
    bool AddFriend(int fromuid, int touid, const std::string &bakname, const std::string &desc,
                   std::vector<std::shared_ptr<AddFriendmsg>> &chat_datas);
    // 获取 touid 接收到的所有 好友申请
    bool GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>> &applyList, int begin,
                      int limit = 10);

    // 获取 self_id 所有的 好友信息
    bool GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> &user_info_list);

    bool GetUserThreads(int64_t userId, int64_t lastId, int pageSize,
                        std::vector<std::shared_ptr<ChatThreadInfo>> &threads, bool &loadMore,
                        int &nextLastId);

    bool CreatePrivateChat(int user1_id, int user2_id, int &thread_id);

    std::shared_ptr<PageResult> LoadChatMsg(int thread_id, int message_id, int page_size);

private:
    std::unique_ptr<MySqlPool> _pool;
};
} // namespace core