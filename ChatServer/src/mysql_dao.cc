#include "mysql_dao.h"
#include "configmgr.h"
#include <boost/lexical_cast.hpp>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <memory>

namespace core
{
MySqlPool::MySqlPool(const std::string &url, const std::string &user, const std::string &pass,
                     const std::string &schema, int poolsize)
    : _url(url), _user(user), _pass(pass), _schema(schema), _poolsize(poolsize), _b_stop(false)
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
            while (!_b_stop) {
                checkConnection();
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
        });
        _check_thread.detach();
    } catch (sql::SQLException &e) {
        LOG_ERROR("mysql pool init failed {}", e.what());
    }
}

void MySqlPool::checkConnection()
{
    std::unique_lock<std::mutex> lock(_mutex);
    size_t queue_size = _conns.size();
    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long timestamp = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    for (size_t processed = 0; processed < queue_size; ++processed) {
        auto conn = std::move(_conns.front());
        _conns.pop();

        Defer defer([this, &conn]() { _conns.push(std::move(conn)); });

        // 解锁后做检查 / 重连逻辑
        if (timestamp - conn->_last_oper_time >= 5) {
            try {
                std::unique_ptr<sql::Statement> stmt(conn->_conn->createStatement());
                stmt->executeQuery("SELECT 1");
                conn->_last_oper_time = timestamp;
            } catch (sql::SQLException &e) {
                std::cout << "Error keeping connection alive: " << e.what() << std::endl;
                // 重新创建连接并替换旧的连接
                sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
                auto *newcon = driver->connect(_url, _user, _pass);
                newcon->setSchema(_schema);
                conn->_conn.reset(newcon);
                conn->_last_oper_time = timestamp;
            }
        }
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