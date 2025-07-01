#include "mysql_dao.h"
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
    : _url(url), _user(user), _pass(pass), _schema(schema), _poolsize(poolsize), _b_stop(false), _fail_count(0)
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
                if(count >= 60){
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
        LOG_DEBUG("mysql connection reconnect success");
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

bool MysqlDao::AddFriendApply(int uid, int touid)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        std::unique_ptr<sql::PreparedStatement> stmt(conn->_conn->prepareStatement(
            "INSERT INTO friend_apply (from_uid, to_uid) values (?,?) "
            "ON DUPLICATE KEY UPDATE from_uid = from_uid, to_uid = to_uid"));

        stmt->setInt(1, uid);
        stmt->setInt(2, touid);

        int rowAffected = stmt->executeUpdate();
        if (rowAffected < 0) {
            return false;
        }

        return true;
    } catch (sql::SQLException &e) {
        LOG_ERROR("GetUser failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
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
        LOG_ERROR("GetUser failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return false;
    }
}

bool MysqlDao::AddFriend(int fromuid, int touid)
{
    auto conn = _pool->getConnection();
    if (conn == nullptr) {
        LOG_ERROR("DB connection unavailable");
        return false;
    }
    Defer defer([this, &conn]() { _pool->returnConnection(std::move(conn)); });

    try {
        conn->_conn->setAutoCommit(false);

        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->_conn->prepareStatement("INSERT INTO friend (self_id, friend_id) VALUES (?, ?)"));

        stmt->setInt(1, fromuid);
        stmt->setInt(2, touid);

        int update_cols = stmt->executeUpdate();
        if (update_cols < 0) {
            conn->_conn->rollback();
            return false;
        }

        stmt->setInt(1, touid);
        stmt->setInt(2, fromuid);
        update_cols = stmt->executeUpdate();
        if (update_cols < 0) {
            conn->_conn->rollback();
            return false;
        }
        LOG_INFO("fromuid:{}, touid:{}, add friend success", fromuid, touid);

        conn->_conn->commit();
        return true;
    } catch (sql::SQLException &e) {
        if (conn) {
            conn->_conn->rollback();
        }
        LOG_ERROR("GetUser failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return false;
    }
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
        LOG_ERROR("GetUser failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
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
        LOG_ERROR("GetUser failed: {} (Code: {}, State: {})", e.what(), e.getErrorCode(),
                  e.getSQLState());
        return false;
    }
}

} // namespace core