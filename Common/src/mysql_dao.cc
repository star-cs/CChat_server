#include "mysql_dao.h"
#include "common.h"
#include "configmgr.h"
#include <boost/lexical_cast.hpp>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <memory>
#include <mutex>
#include <mysql_driver.h>

namespace core
{
MySqlPool::MySqlPool(const std::string &url, const std::string &user, const std::string &pass,
                     const std::string &schema, int poolsize)
    : _url(url), _user(user), _pass(pass), _schema(schema), _poolsize(poolsize), _b_stop(false),
      _fail_count(0)
{
    try {
        assert(_poolsize > 0 && _poolsize < 100);
        for (int i = 0; i < _poolsize; ++i) {
            sql::mysql::MySQL_Driver *driver = sql::mysql::get_driver_instance();
            sql::Connection *conn = driver->connect(_url, _user, _pass);
            conn->setSchema(_schema);

            auto currentTime = std::chrono::system_clock::now().time_since_epoch();
            long long timestamp =
                std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
            _conns.push(std::make_unique<SqlConnection>(conn, timestamp));
        }

        LOG_INFO("MySQL连接建立成功，连接池大小：{}", _conns.size());
        _check_thread = std::thread([this]() {
            int count = 0;
            while (!_b_stop) {
                if (count >= 60) {
                    count = 0;
                    checkConnection();
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
                count++;
            }
        });
        _check_thread.detach();
    } catch (sql::SQLException &e) {
        LOG_ERROR("mysql pool init failed {}", e.what());
    }
}

void MySqlPool::checkConnection()
{
    // 1. 读取 处理数
    size_t queue_size = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        queue_size = _conns.size();
    }
    // 2. 已处理数量
    std::size_t processed = 0;

    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    while (processed < queue_size) {
        std::unique_ptr<SqlConnection> conn;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_conns.empty() || _b_stop) {
                break;
            }
            conn = std::move(_conns.front());
            _conns.pop();
        }

        bool healthy = true;
        // 解锁后做检查 / 重连逻辑
        if (timestamp - conn->_last_oper_time >= 5) {
            try {
                std::unique_ptr<sql::Statement> stmt(conn->_conn->createStatement());
                stmt->executeQuery("SELECT 1");
                conn->_last_oper_time = timestamp;
            } catch (sql::SQLException &e) {
                std::cout << "Error keeping connection alive: " << e.what() << std::endl;
                healthy = false;
                _fail_count++;
            }

            if (healthy) {
                std::lock_guard<std::mutex> lock(_mutex);
                _conns.push(std::move(conn));
                _cond.notify_one();
            }
        }
        processed++;
    }
    while (_fail_count > 0) {
        auto b_res = reconnect(timestamp);
        if (b_res) {
            _fail_count--;
        } else {
            break;
        }
    }
}

bool MySqlPool::reconnect(long long timestamp)
{
    try {
        sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
        auto *con = driver->connect(_url, _user, _pass);

        con->setSchema(_schema);

        auto newConn = std::make_unique<SqlConnection>(con, timestamp);
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _conns.push(std::move(newConn));
        }
        LOG_INFO("mysql connection reconnect success");
        return true;
    } catch (sql::SQLException &e) {
        LOG_ERROR("Reconnect failed, error is {}", e.what());
        return false;
    }
}

std::unique_ptr<SqlConnection> MySqlPool::getConnection()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _cond.wait(lock, [this]() {
        if (_b_stop)
            return true;
        return !_conns.empty();
    });

    if (_b_stop) {
        return nullptr;
    }

    std::unique_ptr<SqlConnection> conn(std::move(_conns.front()));
    _conns.pop();
    return conn;
}

void MySqlPool::returnConnection(std::unique_ptr<SqlConnection> conn)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (_b_stop) {
        return;
    }
    _conns.push(std::move(conn));
    _cond.notify_one();
}

void MySqlPool::Close()
{
    _b_stop = true;
    _cond.notify_all();
}

MySqlPool::~MySqlPool()
{
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_conns.empty()) {
        _conns.pop();
    }
}

int MysqlDao::RegUser(const std::string &name, const std::string &email, const std::string &pwd)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    try {

        // 准备调用存储过程
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->_conn->prepareStatement("CALL reg_user(?,?,?,@result)"));
        stmt->setString(1, name);
        stmt->setString(2, email);
        stmt->setString(3, pwd);

        // 由于PreparedStatement不直接支持注册输出参数，我们需要使用会话变量或其他方法来获取输出参数的值

        // 执行存储过程
        stmt->execute();
        // 如果存储过程设置了会话变量或有其他方式获取输出参数的值，你可以在这里执行SELECT查询来获取它们
        // 例如，如果存储过程设置了一个会话变量@result来存储输出结果，可以这样获取：
        std::unique_ptr<sql::Statement> stmtResult(conn->_conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
        if (res->next()) {
            int result = res->getInt("result");
            std::cout << "Result: " << result << std::endl;
            _pool->returnConnection(std::move(conn));
            return result;
        }
        // 没有
        _pool->returnConnection(std::move(conn));
        return -1;
    } catch (sql::SQLException &e) {
        _pool->returnConnection(std::move(conn));
        std::cerr << "SQLException: " << e.what();
        std::cerr << " (MySQL error code: " << e.getErrorCode();
        std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        return -1;
    }
}

int MysqlDao::RegUserTransaction(const std::string &name, const std::string &email,
                                 const std::string &pwd, const std::string &icon)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });
    try {
        // 开启事务。关闭自动提交，手动控制事务提交与回滚
        conn->_conn->setAutoCommit(false);

        // 1. 查询邮件是否重复
        std::unique_ptr<sql::PreparedStatement> pstmt_email(
            conn->_conn->prepareStatement("SELECT 1 FROM user WHERE email = ?"));
        pstmt_email->setString(1, email);

        std::unique_ptr<sql::ResultSet> res_email(pstmt_email->executeQuery());
        auto email_exist = res_email->next();
        if (email_exist) {
            conn->_conn->rollback(); // 回退
            std::cout << "email " << email << " exist";
            return 0;
        }

        // 2. 查询账号是否重复
        std::unique_ptr<sql::PreparedStatement> pstmt_user(
            conn->_conn->prepareStatement("SELECT 1 FROM user WHERE name = ?"));
        pstmt_user->setString(1, name);

        std::unique_ptr<sql::ResultSet> res_user(pstmt_user->executeQuery());
        auto user_exist = res_user->next();
        if (user_exist) {
            conn->_conn->rollback(); // 回退
            std::cout << "name " << name << " exist";
            return 0;
        }

        // 3. 更新 用户id
        // user_id 自增
        std::unique_ptr<sql::PreparedStatement> pstmt_uid(
            conn->_conn->prepareStatement("UPDATE user_id SET id = id + 1"));

        pstmt_uid->executeUpdate();

        pstmt_uid.reset(conn->_conn->prepareStatement("SELECT id FROM user_id"));

        std::unique_ptr<sql::ResultSet> res_uid(pstmt_uid->executeQuery());
        int newId = 0;
        if (res_uid->next()) {
            newId = res_uid->getInt("id");
        } else {
            std::cout << "select id from user_id failed" << std::endl;
            conn->_conn->rollback();
            return -1;
        }

        // 4. 插入 user 信息
        std::unique_ptr<sql::PreparedStatement> pstmt_insert(conn->_conn->prepareStatement(
            "INSERT INTO user (uid, name, email, pwd, nick, icon) VALUES (?, ?, ?, ?, ?, ?)"));

        pstmt_insert->setInt(1, newId);
        pstmt_insert->setString(2, name);
        pstmt_insert->setString(3, email);
        pstmt_insert->setString(4, pwd);
        pstmt_insert->setString(5, name);
        pstmt_insert->setString(6, icon);

        pstmt_insert->executeUpdate();

        // 5. 提交 事务
        conn->_conn->commit();
        std::cout << "new user insert into user table success" << std::endl;
        return newId;
    } catch (sql::SQLException &e) {
        // 如果发生错误，回滚事务
        if (conn) {
            conn->_conn->rollback();
        }
        LOG_ERROR("SQLException: {}", e.what());
        LOG_ERROR("(MySQL error code: {}", e.getErrorCode());
        LOG_ERROR(", SQLState: {} )", e.getSQLState());
        return -1;
    }
}

bool MysqlDao::CheckEmail(const std::string &name, const std::string &email)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });
    try {
        // 1. 检查是否有email 和 name 同时匹配的数据
        std::unique_ptr<sql::PreparedStatement> stmt_email(
            conn->_conn->prepareStatement("SELECT 1 FROM user WHERE name = ? and email = ?"));
        stmt_email->setString(1, name);
        stmt_email->setString(2, email);
        std::unique_ptr<sql::ResultSet> res_email(stmt_email->executeQuery());
        if (res_email->next()) {
            return true;
        }

        return false;
    } catch (sql::SQLException &e) {
        LOG_ERROR("SQLException: {}", e.what());
        LOG_ERROR("(MySQL error code: {}", e.getErrorCode());
        LOG_ERROR(", SQLState: {} )", e.getSQLState());
        return false;
    }

    return true;
}

bool MysqlDao::UpdatePwd(const std::string &email, const std::string &newpwd)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }

    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });
    try {
        conn->_conn->setAutoCommit(false);

        std::unique_ptr<sql::PreparedStatement> stmt_up(
            conn->_conn->prepareStatement("UPDATE user SET pwd = ? WHERE email = ?"));
        stmt_up->setString(1, newpwd);
        stmt_up->setString(2, email);

        int updateCount = stmt_up->executeUpdate();
        LOG_INFO("email: {}, update pwd rows: {}", email, updateCount);
        conn->_conn->commit();
    } catch (sql::SQLException &e) {
        if (conn) {
            conn->_conn->rollback();
        }
        LOG_ERROR("SQLException: {}", e.what());
        LOG_ERROR("(MySQL error code: {}", e.getErrorCode());
        LOG_ERROR(", SQLState: {} )", e.getSQLState());
        return false;
    }
    return true;
}

bool MysqlDao::CheckPwd(const std::string &email, const std::string &pwd, UserInfo &userInfo)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        auto stmt =
            conn->_conn->prepareStatement("SELECT uid, name, email, pwd FROM user WHERE email = ?");
        stmt->setString(1, email);

        auto res = std::unique_ptr<sql::ResultSet>(stmt->executeQuery());
        if (res->next()) {
            std::string storedHash = res->getString("pwd");
            if (pwd == storedHash) {
                userInfo.uid = res->getInt("uid");
                userInfo.name = res->getString("name");
                userInfo.email = res->getString("email");
                return true;
            }
        }
        return false; // 邮箱不存在或密码错误
    } catch (const sql::SQLException &e) {
        LOG_ERROR("CheckPwd failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return false;
    }
}

MysqlDao::MysqlDao()
{
    auto &configMgr = ConfigMgr::GetInstance();
    auto poolnum = configMgr["Mysql"]["MysqlConnPoolNum"];
    int pool_num = boost::lexical_cast<int>(poolnum);
    if (pool_num < 1 || pool_num > 8) {
        std::cout << "config.ini [Mysql][MysqlConnPoolNum] Invalid pool_num value: " << poolnum
                  << std::endl;
        pool_num = 2;
    }
    const auto &host = configMgr["Mysql"]["host"];
    const auto &port = configMgr["Mysql"]["port"];
    const auto &passwd = configMgr["Mysql"]["passwd"];
    const auto &user = configMgr["Mysql"]["user"];
    const auto &schema = configMgr["Mysql"]["schema"];
    _pool.reset(new MySqlPool(host + ":" + port, user, passwd, schema, pool_num));
}

MysqlDao::~MysqlDao()
{
    _pool->Close();
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(int uid)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return nullptr;
    }

    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        std::unique_ptr<sql::PreparedStatement> stmt_user(
            conn->_conn->prepareStatement("SELECT * FROM user WHERE uid = ?"));
        stmt_user->setInt(1, uid);

        std::unique_ptr<sql::ResultSet> res(stmt_user->executeQuery());
        std::shared_ptr<UserInfo> user_ptr = nullptr;
        while (res->next()) {
            user_ptr.reset(new UserInfo);
            user_ptr->name = res->getString("name");
            user_ptr->pwd = res->getString("pwd");
            user_ptr->uid = uid;
            user_ptr->email = res->getString("email");
            user_ptr->nick = res->getString("nick");
            user_ptr->desc = res->getString("desc");
            user_ptr->sex = res->getInt("sex");
            user_ptr->icon = res->getString("icon");
            break;
        }
        return user_ptr;
    } catch (sql::SQLException &e) {
        LOG_ERROR("GetUser failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return nullptr;
    }
}

std::shared_ptr<UserInfo> MysqlDao::GetUser(const std::string &name)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return nullptr;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->_conn->prepareStatement("SELECT * FROM user WHERE name = ?"));
        stmt->setString(1, name);

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        std::shared_ptr<UserInfo> user_ptr = nullptr;
        while (res->next()) {
            user_ptr.reset(new UserInfo);
            user_ptr->uid = res->getInt("uid");
            user_ptr->email = res->getString("email");
            user_ptr->name = name;
            user_ptr->pwd = res->getString("pwd");
            user_ptr->icon = res->getString("icon");
            break;
        }
        return user_ptr;
    } catch (sql::SQLException &e) {
        LOG_ERROR("GetUser failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return nullptr;
    }
}

bool MysqlDao::AddFriendApply(int uid, int touid, const std::string &desc,
                              const std::string &bakname)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->_conn->prepareStatement(
            "INSERT INTO friend_apply (from_uid, to_uid, descs, bakname) "
            "values (?,?,?,?) "
            "ON DUPLICATE KEY UPDATE from_uid = from_uid, to_uid = to_uid, descs = ?, bakname = "
            "?"));
        pstmt->setInt(1, uid); // from id
        pstmt->setInt(2, touid);
        pstmt->setString(3, desc);
        pstmt->setString(4, bakname);
        pstmt->setString(5, desc);
        pstmt->setString(6, bakname);

        int rowAffected = pstmt->executeUpdate();
        if (rowAffected < 0) {
            return false;
        }
        return true;
    } catch (sql::SQLException &e) {
        LOG_ERROR("AddFriendApply failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return false;
    }
}

bool MysqlDao::AuthFriendApply(int fromuid, int touid)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        conn->_conn->setAutoCommit(false);
        std::unique_ptr<sql::PreparedStatement> stmt(conn->_conn->prepareStatement(
            "UPDATE friend_apply SET status = 1 WHERE from_uid = ? AND to_uid = ?"));

        stmt->setInt(1, fromuid);
        stmt->setInt(2, touid);

        int update_cols = stmt->executeUpdate();
        if (update_cols < 0) {
            conn->_conn->rollback();
            return false;
        }
        LOG_INFO("auth friend success, from_uid:{}, to_uid:{}", fromuid, touid);

        conn->_conn->commit();
        return true;
    } catch (sql::SQLException &e) {
        if (conn) {
            conn->_conn->rollback();
        }
        LOG_ERROR("AuthFriendApply failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return false;
    }
}

bool MysqlDao::AddFriend(int fromuid, int touid, const std::string &bakname,
                         const std::string &desc,
                         std::vector<std::shared_ptr<AddFriendmsg>> &chat_datas)
{
    const int MAX_RETRY = 3;
    int retry_count = 0;
    bool success = false;

    while (retry_count < MAX_RETRY && !success) {
        auto conn = _pool->getConnection();
        if (conn == nullptr) {
            LOG_ERROR("DB connection unavailable");
            return false;
        }
        Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

        try {
            conn->_conn->setAutoCommit(false);

            std::string reverse_back;
            std::string apply_desc;

            // 1. 使用乐观锁更新状态
            {
                std::unique_ptr<sql::PreparedStatement> updStmt(
                    conn->_conn->prepareStatement("UPDATE friend_apply "
                                                  "SET status = 1 "
                                                  "WHERE from_uid = ? AND to_uid = ? "
                                                  "AND status = 0"));

                updStmt->setInt(1, touid);
                updStmt->setInt(2, fromuid);

                int updated = updStmt->executeUpdate();
                if (updated != 1) {
                    conn->_conn->rollback();
                    LOG_WARN("No pending friend application found or already processed");
                    return false;
                }
            }

            // 2. 查询申请数据
            {
                std::unique_ptr<sql::PreparedStatement> selStmt(
                    conn->_conn->prepareStatement("SELECT bakname, descs "
                                                  "FROM friend_apply "
                                                  "WHERE from_uid = ? AND to_uid = ?"));
                selStmt->setInt(1, touid);
                selStmt->setInt(2, fromuid);

                std::unique_ptr<sql::ResultSet> res(selStmt->executeQuery());
                if (res->next()) {
                    reverse_back = res->getString("bakname");
                    apply_desc = res->getString("descs");
                } else {
                    conn->_conn->rollback();
                    LOG_ERROR("Friend application data missing after update");
                    return false;
                }
            }

            // 3. 插入双向好友关系
            {
                // 插入认证方好友数据
                std::unique_ptr<sql::PreparedStatement> pstmt(conn->_conn->prepareStatement(
                    "INSERT IGNORE INTO friend(self_id, friend_id, bakname) "
                    "VALUES (?, ?, ?) "));
                pstmt->setInt(1, fromuid);
                pstmt->setInt(2, touid);
                pstmt->setString(3, bakname);
                if (pstmt->executeUpdate() < 0) {
                    conn->_conn->rollback();
                    return false;
                }

                // 插入申请方好友数据
                std::unique_ptr<sql::PreparedStatement> pstmt2(conn->_conn->prepareStatement(
                    "INSERT IGNORE INTO friend(self_id, friend_id, bakname) "
                    "VALUES (?, ?, ?) "));
                pstmt2->setInt(1, touid);
                pstmt2->setInt(2, fromuid);
                pstmt2->setString(3, reverse_back);
                if (pstmt2->executeUpdate() < 0) {
                    conn->_conn->rollback();
                    return false;
                }
            }

            // 4. 创建 chat_thread 并获取ID
            long long threadId = 0;
            {
                std::unique_ptr<sql::PreparedStatement> chat_thread_stmt(
                    conn->_conn->prepareStatement(
                        "INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW())"));

                if (chat_thread_stmt->executeUpdate() < 0) {
                    conn->_conn->rollback();
                    return false;
                }

                // 使用传统方法获取最后插入ID
                std::unique_ptr<sql::Statement> idStmt(conn->_conn->createStatement());
                std::unique_ptr<sql::ResultSet> idRes(
                    idStmt->executeQuery("SELECT LAST_INSERT_ID()"));
                if (idRes->next()) {
                    threadId = idRes->getInt64(1);
                } else {
                    conn->_conn->rollback();
                    LOG_ERROR("Failed to get chat_thread ID");
                    return false;
                }
            }

            // 5. 写入私聊记录
            {
                std::unique_ptr<sql::PreparedStatement> private_chat_stmt(
                    conn->_conn->prepareStatement("INSERT INTO private_chat (thread_id, user1_id, "
                                                  "user2_id, created_at) VALUES (?, ?, ?, NOW())"));
                private_chat_stmt->setInt64(1, threadId);
                private_chat_stmt->setInt(2, std::min(fromuid, touid));
                private_chat_stmt->setInt(3, std::max(fromuid, touid));
                if (private_chat_stmt->executeUpdate() < 0) {
                    conn->_conn->rollback();
                    return false;
                }
            }
            // 6. 插入初始消息（如果有）
            auto insertMessage = [&](int sender, int receiver, const std::string &content) -> bool {
                if (content.empty())
                    return true;

                std::unique_ptr<sql::PreparedStatement> msgStmt(conn->_conn->prepareStatement(
                    "INSERT INTO chat_message(thread_id, sender_id, recv_id, content, "
                    "created_at, updated_at, status) VALUES (?, ?, ?, ?, NOW(), NOW(), ?)"));

                msgStmt->setInt64(1, threadId);
                msgStmt->setInt(2, sender);
                msgStmt->setInt(3, receiver);
                msgStmt->setString(4, content);
                msgStmt->setInt(5, 0);

                if (msgStmt->executeUpdate() < 0) {
                    return false;
                }

                std::unique_ptr<sql::Statement> idStmt(conn->_conn->createStatement());
                std::unique_ptr<sql::ResultSet> idRes(
                    idStmt->executeQuery("SELECT LAST_INSERT_ID()"));
                if (idRes->next()) {
                    auto messageId = idRes->getInt64(1);
                    auto tx_data = std::make_shared<AddFriendmsg>();
                    tx_data->sender_id = sender;
                    tx_data->msg_id = messageId;
                    tx_data->msgcontent = content;
                    tx_data->thread_id = threadId;
                    tx_data->unique_id = "";
                    chat_datas.push_back(tx_data);
                    return true;
                }
                return false;
            };

            // 插入申请方消息
            if (!apply_desc.empty() && !insertMessage(touid, fromuid, apply_desc)) {
                conn->_conn->rollback();
                return false;
            }

            // 插入认证方消息
            if (!desc.empty() && !insertMessage(fromuid, touid, desc)) {
                conn->_conn->rollback();
                return false;
            }

            conn->_conn->commit();
            LOG_INFO("AddFriend succeeded for users {} and {}", fromuid, touid);
            success = true;

        } catch (sql::SQLException &e) {
            if (conn) {
                conn->_conn->rollback();
            }

            // 处理锁超时错误 (1205)
            if (e.getErrorCode() == 1205) {
                retry_count++;
                LOG_WARN("Lock timeout (retry {} of {}): {}", retry_count, MAX_RETRY, e.what());
                // 指数退避策略
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << retry_count)));
            } else {
                LOG_ERROR("AddFriend failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                          e.getSQLState());
                return false;
            }
        }
    }

    return success;
}

// 获取 touid 接收到的所有 好友申请
bool MysqlDao::GetApplyList(int touid, std::vector<std::shared_ptr<ApplyInfo>> &applyList,
                            int begin, int limit)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        std::unique_ptr<sql::PreparedStatement> stmt(conn->_conn->prepareStatement(
            "SELECT apply.from_uid, apply.status, user.name, user.nick, user.sex, user.icon "
            "FROM friend_apply as apply "
            "JOIN user on apply.from_uid = user.uid "
            "WHERE apply.to_uid = ? and apply.id > ? "
            "ORDER BY apply.id ASC LIMIt ?"));

        stmt->setInt(1, touid);
        stmt->setInt(2, begin);
        stmt->setInt(3, limit);

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            auto name = res->getString("name");
            auto uid = res->getInt("from_uid");
            auto status = res->getInt("status");
            auto nick = res->getString("nick");
            auto sex = res->getInt("sex");
            auto icon = res->getString("icon");
            auto apply_ptr = std::make_shared<ApplyInfo>(uid, name, "", icon, nick, sex, status);
            applyList.push_back(apply_ptr);
        }
        return true;

    } catch (sql::SQLException &e) {
        LOG_ERROR("GetApplyList failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return false;
    }
}

// 获取 self_id 所有的 好友信息
bool MysqlDao::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> &user_info_list)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->_conn->prepareStatement("SELECT * FROM friend where self_id = ?"));

        stmt->setInt(1, self_id);

        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            auto friend_id = res->getInt("friend_id");
            auto user_info = GetUser(friend_id);
            if (user_info == nullptr) {
                continue;
            }
            user_info_list.push_back(user_info);
        }
        return true;

    } catch (sql::SQLException &e) {
        LOG_ERROR("GetFriendList failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return false;
    }
}

bool MysqlDao::GetUserThreads(int64_t userId, int64_t lastId, int pageSize,
                              std::vector<std::shared_ptr<ChatThreadInfo>> &threads, bool &loadMore,
                              int &nextLastId)
{
    // 初始状态
    loadMore = false;
    nextLastId = lastId;
    threads.clear();

    auto con = _pool->getConnection();
    if (!con) {
        return false;
    }
    Defer defer([this, &con]() { _pool->returnConnection(std::move(con)); });
    auto &conn = con->_conn;

    try {
        // 准备分页查询：CTE + UNION ALL + ORDER + LIMIT N+1
        std::string sql = "WITH all_threads AS ( "
                          "  SELECT thread_id, 'private' AS type, user1_id, user2_id "
                          "    FROM private_chat "
                          "   WHERE (user1_id = ? OR user2_id = ?) "
                          "     AND thread_id > ? "
                          "  UNION ALL "
                          "  SELECT thread_id, 'group' AS type, 0 AS user1_id, 0 AS user2_id "
                          "    FROM group_chat_member "
                          "   WHERE user_id = ? "
                          "     AND thread_id > ? "
                          ") "
                          "SELECT thread_id, type, user1_id, user2_id "
                          "  FROM all_threads "
                          " ORDER BY thread_id "
                          " LIMIT ?;";

        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sql));

        // 绑定参数：? 对应 (userId, userId, lastId, userId, lastId, pageSize+1)
        int idx = 1;
        pstmt->setInt64(idx++, userId);     // private.user1_id
        pstmt->setInt64(idx++, userId);     // private.user2_id
        pstmt->setInt64(idx++, lastId);     // private.thread_id > lastId
        pstmt->setInt64(idx++, userId);     // group.user_id
        pstmt->setInt64(idx++, lastId);     // group.thread_id > lastId
        pstmt->setInt(idx++, pageSize + 1); // LIMIT pageSize+1

        // 执行
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        // 先把所有行读到临时容器
        std::vector<std::shared_ptr<ChatThreadInfo>> tmp;
        while (res->next()) {
            auto cti = std::make_shared<ChatThreadInfo>();
            cti->_thread_id = res->getInt64("thread_id");
            cti->_type = res->getString("type");
            cti->_user1_id = res->getInt64("user1_id");
            cti->_user2_id = res->getInt64("user2_id");
            tmp.push_back(cti);
        }

        // 判断是否多取到一条
        if ((int)tmp.size() > pageSize) {
            loadMore = true;
            tmp.pop_back(); // 丢掉第 pageSize+1 条
        }

        // 如果还有数据，更新 nextLastId 为最后一条的 thread_id
        if (!tmp.empty()) {
            nextLastId = tmp.back()->_thread_id;
        }

        // 移入输出向量
        threads = std::move(tmp);
    } catch (sql::SQLException &e) {
        LOG_ERROR("GetUserThreads failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return false;
    }

    return true;
}

bool MysqlDao::CreatePrivateChat(int user1_id, int user2_id, int &thread_id)
{
    const int MAX_RETRY = 3;
    int retry_count = 0;
    bool success = false;

    // 确保 user1_id < user2_id 的排序
    int uid1 = std::min(user1_id, user2_id);
    int uid2 = std::max(user1_id, user2_id);

    while (retry_count < MAX_RETRY && !success) {
        auto conn = _pool->getConnection();
        if (conn == nullptr) {
            LOG_ERROR("DB connection unavailable");
            return false;
        }
        Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

        try {
            // 1. 先尝试无锁查询是否已存在私聊记录
            std::string select_sql = "SELECT thread_id FROM private_chat "
                                     "WHERE user1_id = ? AND user2_id = ?";
            std::unique_ptr<sql::PreparedStatement> select_stmt(
                conn->_conn->prepareStatement(select_sql));
            select_stmt->setInt(1, uid1);
            select_stmt->setInt(2, uid2);

            std::unique_ptr<sql::ResultSet> res(select_stmt->executeQuery());
            if (res->next()) {
                thread_id = res->getInt("thread_id");
                return true; // 记录已存在，直接返回
            }

            // 2. 不存在记录，开始事务
            conn->_conn->setAutoCommit(false);

            // 3. 创建 chat_thread
            long long new_thread_id = 0;
            {
                std::unique_ptr<sql::PreparedStatement> thread_stmt(conn->_conn->prepareStatement(
                    "INSERT INTO chat_thread (type, created_at) VALUES ('private', NOW())"));

                if (thread_stmt->executeUpdate() <= 0) {
                    conn->_conn->rollback();
                    LOG_ERROR("Failed to insert chat_thread");
                    return false;
                }

                // 获取新插入的 thread_id
                std::unique_ptr<sql::Statement> id_stmt(conn->_conn->createStatement());
                std::unique_ptr<sql::ResultSet> id_res(
                    id_stmt->executeQuery("SELECT LAST_INSERT_ID()"));
                if (id_res->next()) {
                    new_thread_id = id_res->getInt64(1);
                } else {
                    conn->_conn->rollback();
                    LOG_ERROR("Failed to get chat_thread ID");
                    return false;
                }
            }

            // 4. 尝试插入 private_chat 记录
            try {
                std::string insert_sql =
                    "INSERT INTO private_chat (thread_id, user1_id, user2_id, created_at) "
                    "VALUES (?, ?, ?, NOW())";

                std::unique_ptr<sql::PreparedStatement> insert_stmt(
                    conn->_conn->prepareStatement(insert_sql));
                insert_stmt->setInt64(1, new_thread_id);
                insert_stmt->setInt(2, uid1);
                insert_stmt->setInt(3, uid2);

                if (insert_stmt->executeUpdate() <= 0) {
                    throw sql::SQLException("Insert failed", "HY000", 0);
                }

                conn->_conn->commit();
                thread_id = static_cast<int>(new_thread_id);
                LOG_INFO("Created new private chat: thread_id={}, users=[{}, {}]", thread_id, uid1,
                         uid2);
                success = true;
            } catch (sql::SQLException &e) {
                // 处理唯一约束冲突 (ER_DUP_ENTRY)
                if (e.getErrorCode() == 1062) {
                    conn->_conn->rollback();
                    LOG_WARN("Duplicate private chat detected, retrying...");
                    retry_count++;
                    // 指数退避
                    std::this_thread::sleep_for(std::chrono::milliseconds(50 * (1 << retry_count)));
                } else {
                    // 其他错误
                    conn->_conn->rollback();
                    LOG_ERROR("Insert private_chat failed: {} (Code: {}, State: {})", e.what(),
                              e.getErrorCode(), e.getSQLState());
                    return false;
                }
            }
        } catch (sql::SQLException &e) {
            // 处理锁超时错误 (1205)
            if (e.getErrorCode() == 1205) {
                retry_count++;
                LOG_WARN("Lock timeout (retry {} of {}): {}", retry_count, MAX_RETRY, e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (1 << retry_count)));
            } else {
                LOG_ERROR("CreatePrivateChat failed: {} (Code: {}, State: {})", e.what(),
                          e.getErrorCode(), e.getSQLState());
                return false;
            }
        }
    }

    return success;
}

std::shared_ptr<PageResult> MysqlDao::LoadChatMsg(int thread_id, int message_id, int page_size)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return nullptr;
    }
    std::shared_ptr<PageResult> pageResult = std::make_shared<PageResult>();
    pageResult->load_more = false;
    pageResult->next_cursor = 0;

    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->_conn->prepareStatement("SELECT message_id, thread_id, sender_id, recv_id, "
                                          "content, created_at, updated_at, status "
                                          "FROM chat_message "
                                          "WHERE thread_id = ? AND message_id > ? "
                                          "ORDER BY message_id ASC " // ASC升序，DESC降序
                                          "LIMIT ? "));
        stmt->setInt(1, thread_id);
        stmt->setInt(2, message_id);
        stmt->setInt(3, page_size+1);
                
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while(res->next()){
            ChatMessage msg;
            msg.message_id = res->getInt("message_id");
            msg.thread_id = res->getInt("thread_id");
            msg.sender_id = res->getInt("sender_id");
            msg.recv_id = res->getInt("recv_id");
            msg.content = res->getString("content");
            msg.chat_time = res->getString("created_at");
            msg.status = res->getInt("status");
            pageResult->messages.push_back(msg);
        }

        if(pageResult->messages.size() == page_size+1){
            pageResult->load_more = true;
            pageResult->messages.pop_back(); // 删除最后一个
            pageResult->next_cursor = pageResult->messages.back().message_id;
        }

        return pageResult;

    } catch (sql::SQLException &e) {
        LOG_ERROR("LoadChatMsg failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return nullptr;
    }
}

} // namespace core