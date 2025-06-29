#include "redis_mgr.h"
#include "configmgr.h"
#include "dist_lock.h"
#include "logger.hpp"
#include <string>
namespace core
{
RedisConnPool::RedisConnPool(std::size_t pool_size, const char *host, unsigned short port,
                             const char *passwd)
    : _pool_size(pool_size), _host(host), _port(port), _b_stop(false)
{
    for (std::size_t i = 0; i < _pool_size; ++i) {
        redisContext *c = redisConnect(_host, _port);
        if (!c || c->err) {
            printf("Connect to redisServer failed: %s\n", c ? c->errstr : "connection error");
            if (c)
                redisFree(c);
            continue;
        }

        // 非空密码才进行认证
        if (passwd && strlen(passwd) > 0) {
            redisReply *r = (redisReply *)redisCommand(c, "AUTH %s", passwd);
            if (!r || r->type == REDIS_REPLY_ERROR) {
                printf("Redis认证失败！\n");
                redisFree(c);
                if (r)
                    freeReplyObject(r);
                continue;
            }
            freeReplyObject(r);
        }
        _conns.push(c);
    }
    LOG_INFO("Redis连接创建成功，连接池大小：{}", _conns.size());
}

RedisConnPool::~RedisConnPool()
{
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_conns.empty()) {
        auto c = _conns.front();
        _conns.pop();
        redisFree(c);
    }
}

redisContext *RedisConnPool::getConnection()
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
    auto *c = _conns.front();
    _conns.pop();
    return c;
}

void RedisConnPool::returnConnection(redisContext *c)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (_b_stop) {
        redisFree(c);
        return;
    }
    _conns.push(c);
    _cond.notify_one();
}

void RedisConnPool::Close()
{
    _b_stop = true;
    _cond.notify_all();
}

RedisMgr::RedisMgr()
{
    auto &configMgr = ConfigMgr::GetInstance();
    std::string redisConnPoolNum = configMgr["Redis"]["RedisConnPoolNum"];
    std::string host = configMgr["Redis"]["host"];
    std::string port = configMgr["Redis"]["port"];
    std::string pwd = configMgr["Redis"]["pwd"];

    _redisConnPool.reset(new RedisConnPool(boost::lexical_cast<std::size_t>(redisConnPoolNum),
                                           host.c_str(), boost::lexical_cast<unsigned short>(port),
                                           pwd.c_str()));

    // 安全日志：隐藏密码
    LOG_INFO("RedisMgr host: {}, port: {}, pwd: {}", host, port,
             (pwd.empty() ? "[empty]" : "******"));
}

RedisMgr::~RedisMgr()
{
    Close();
}

/*

    重点记住这 4 类 reply->type：
    REDIS_REPLY_STRING：返回字符串（比如 GET、LPOP）
    REDIS_REPLY_INTEGER：返回整数（比如 EXISTS、HSET、LPUSH）
    REDIS_REPLY_STATUS：状态型 OK 字符串（比如 SET）
    REDIS_REPLY_NIL：值不存在（比如 GET、LPOP 空，HGET 不存在）

    */
bool RedisMgr::Get(const std::string &key, std::string &value)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply *)redisCommand(connect, "GET %s", key.c_str());
    if (reply == NULL) {
        std::cout << "[ GET  " << key << " ] failed" << std::endl;
        // freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_STRING) {
        std::cout << "[ GET  " << key << " ] failed" << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);

    std::cout << "Succeed to execute command [ GET " << key << "  ]" << std::endl;
    _redisConnPool->returnConnection(connect);
    return true;
}

bool RedisMgr::Set(const std::string &key, const std::string &value)
{
    // 执行redis命令行
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply *)redisCommand(connect, "SET %s %s", key.c_str(), value.c_str());

    // 如果返回NULL则说明执行失败
    if (NULL == reply) {
        std::cout << "Execut command [ SET " << key << "  " << value << " ] failure ! "
                  << std::endl;
        // freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    // 如果执行失败则释放连接
    if (!(reply->type == REDIS_REPLY_STATUS
          && (strcmp(reply->str, "OK") == 0 || strcmp(reply->str, "ok") == 0))) {
        std::cout << "Execut command [ SET " << key << "  " << value << " ] failure ! "
                  << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    // 执行成功 释放redisCommand执行后返回的redisReply所占用的内存
    freeReplyObject(reply);
    std::cout << "Execut command [ SET " << key << "  " << value << " ] success ! " << std::endl;
    _redisConnPool->returnConnection(connect);
    return true;
}

bool RedisMgr::LPush(const std::string &key, const std::string &value)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply *)redisCommand(connect, "LPUSH %s %s", key.c_str(), value.c_str());
    if (NULL == reply) {
        std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! "
                  << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] failure ! "
                  << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    std::cout << "Execut command [ LPUSH " << key << "  " << value << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _redisConnPool->returnConnection(connect);
    return true;
}

bool RedisMgr::LPop(const std::string &key, std::string &value)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply *)redisCommand(connect, "LPOP %s ", key.c_str());
    if (reply == nullptr) {
        std::cout << "Execut command [ LPOP " << key << " ] failure ! " << std::endl;
        _redisConnPool->returnConnection(connect);
        return false;
    }

    if (reply->type == REDIS_REPLY_NIL) {
        std::cout << "Execut command [ LPOP " << key << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    value = reply->str;
    std::cout << "Execut command [ LPOP " << key << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _redisConnPool->returnConnection(connect);
    return true;
}

bool RedisMgr::RPush(const std::string &key, const std::string &value)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply *)redisCommand(connect, "RPUSH %s %s", key.c_str(), value.c_str());
    if (NULL == reply) {
        std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! "
                  << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer <= 0) {
        std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] failure ! "
                  << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    std::cout << "Execut command [ RPUSH " << key << "  " << value << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _redisConnPool->returnConnection(connect);
    return true;
}
bool RedisMgr::RPop(const std::string &key, std::string &value)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply *)redisCommand(connect, "RPOP %s ", key.c_str());
    if (reply == nullptr) {
        std::cout << "Execut command [ RPOP " << key << " ] failure ! " << std::endl;
        _redisConnPool->returnConnection(connect);
        return false;
    }

    if (reply->type == REDIS_REPLY_NIL) {
        std::cout << "Execut command [ RPOP " << key << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }
    value = reply->str;
    std::cout << "Execut command [ RPOP " << key << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _redisConnPool->returnConnection(connect);
    return true;
}

bool RedisMgr::HSet(const std::string &key, const std::string &hkey, const std::string &value)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply *)redisCommand(connect, "HSET %s %s %s", key.c_str(), hkey.c_str(),
                                            value.c_str());
    if (reply == nullptr) {
        std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value
                  << " ] failure ! " << std::endl;
        _redisConnPool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value
                  << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << value << " ] success ! "
              << std::endl;
    freeReplyObject(reply);
    _redisConnPool->returnConnection(connect);
    return true;
}

bool RedisMgr::HSet(const char *key, const char *hkey, const char *hvalue, size_t hvaluelen)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    const char *argv[4];
    size_t argvlen[4];
    argv[0] = "HSET";
    argvlen[0] = 4;
    argv[1] = key;
    argvlen[1] = strlen(key);
    argv[2] = hkey;
    argvlen[2] = strlen(hkey);
    argv[3] = hvalue;
    argvlen[3] = hvaluelen;

    auto reply = (redisReply *)redisCommandArgv(connect, 4, argv, argvlen);
    if (reply == nullptr) {
        std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue
                  << " ] failure ! " << std::endl;
        _redisConnPool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue
                  << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }
    std::cout << "Execut command [ HSet " << key << "  " << hkey << "  " << hvalue
              << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _redisConnPool->returnConnection(connect);
    return true;
}
std::string RedisMgr::HGet(const std::string &key, const std::string &hkey)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return "";
    }
    const char *argv[3];
    size_t argvlen[3];
    argv[0] = "HGET";
    argvlen[0] = 4;
    argv[1] = key.c_str();
    argvlen[1] = key.length();
    argv[2] = hkey.c_str();
    argvlen[2] = hkey.length();

    auto reply = (redisReply *)redisCommandArgv(connect, 3, argv, argvlen);
    if (reply == nullptr) {
        std::cout << "Execut command [ HGet " << key << " " << hkey << "  ] failure ! "
                  << std::endl;
        _redisConnPool->returnConnection(connect);
        return "";
    }

    if (reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        std::cout << "Execut command [ HGet " << key << " " << hkey << "  ] failure ! "
                  << std::endl;
        _redisConnPool->returnConnection(connect);
        return "";
    }

    std::string value = reply->str;
    freeReplyObject(reply);
    _redisConnPool->returnConnection(connect);
    std::cout << "Execut command [ HGet " << key << " " << hkey << " ] success ! " << std::endl;
    return value;
}

bool RedisMgr::HDel(const std::string &key, const std::string &field)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }

    Defer defer([&connect, this]() { _redisConnPool->returnConnection(connect); });

    redisReply *reply =
        (redisReply *)redisCommand(connect, "HDEL %s %s", key.c_str(), field.c_str());
    if (reply == nullptr) {
        std::cerr << "HDEL command failed" << std::endl;
        return false;
    }

    bool success = false;
    if (reply->type == REDIS_REPLY_INTEGER) {
        success = reply->integer > 0;
    }

    freeReplyObject(reply);
    return success;
}

bool RedisMgr::Del(const std::string &key)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }
    auto reply = (redisReply *)redisCommand(connect, "DEL %s", key.c_str());
    if (reply == nullptr) {
        std::cout << "Execut command [ Del " << key << " ] failure ! " << std::endl;
        _redisConnPool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "Execut command [ Del " << key << " ] failure ! " << std::endl;
        freeReplyObject(reply);
        _redisConnPool->returnConnection(connect);
        return false;
    }

    std::cout << "Execut command [ Del " << key << " ] success ! " << std::endl;
    freeReplyObject(reply);
    _redisConnPool->returnConnection(connect);
    return true;
}

bool RedisMgr::ExistsKey(const std::string &key)
{
    auto connect = _redisConnPool->getConnection();
    if (connect == nullptr) {
        return false;
    }

    auto reply = (redisReply *)redisCommand(connect, "exists %s", key.c_str());
    if (reply == nullptr) {
        std::cout << "Not Found [ Key " << key << " ]  ! " << std::endl;
        _redisConnPool->returnConnection(connect);
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        std::cout << "Not Found [ Key " << key << " ]  ! " << std::endl;
        _redisConnPool->returnConnection(connect);
        freeReplyObject(reply);
        return false;
    }
    std::cout << " Found [ Key " << key << " ] exists ! " << std::endl;
    freeReplyObject(reply);
    _redisConnPool->returnConnection(connect);
    return true;
}

std::string RedisMgr::acquireLock(const std::string &lockName, int lockTimeout, int acquireTimeout)
{
    auto conn = _redisConnPool->getConnection();
    if (conn == nullptr) {
        return "";
    }
    Defer defer([&conn, this]() { _redisConnPool->returnConnection(conn); });

    return DistLock::GetInstance().acquireLock(conn, lockName, lockTimeout, acquireTimeout);
}

bool RedisMgr::releaseLock(const std::string &lockName, const std::string &identifier)
{
    auto conn = _redisConnPool->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this]() { _redisConnPool->returnConnection(conn); });

    return DistLock::GetInstance().releaseLock(conn, lockName, identifier);
}

void RedisMgr::IncreaseCount(std::string server_name)
{
    auto lock_key = LOCK_COUNT;
    auto identifier = acquireLock(LOCK_COUNT, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
    Defer defer([this, identifier, lock_key]() { releaseLock(lock_key, identifier); });

    // 将登录数量增加
    auto rd_res = HGet(LOGIN_COUNT, server_name);

    int count = 0;
    if(!rd_res.empty()){
        count = std::stoi(rd_res);
    }

    count++;

    auto count_str = std::to_string(count);
    HSet(LOGIN_COUNT, server_name, count_str);
}
void RedisMgr::DecreaseCount(std::string server_name)
{
    auto lock_key = LOCK_COUNT;
    auto identifier = acquireLock(LOCK_COUNT, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
    Defer defer([this, identifier, lock_key]() { releaseLock(lock_key, identifier); });

    // 将登录数量增加
    auto rd_res = HGet(LOGIN_COUNT, server_name);

    int count = 0;
    if(!rd_res.empty()){
        count = std::stoi(rd_res);
    }

    count--;

    auto count_str = std::to_string(count);
    HSet(LOGIN_COUNT, server_name, count_str);
}
void RedisMgr::InitCount(std::string server_name)
{
    auto lock_key = LOCK_COUNT;
    auto identifier = acquireLock(LOCK_COUNT, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
    Defer defer([this, identifier, lock_key]() { releaseLock(lock_key, identifier); });

    HSet(LOGIN_COUNT, server_name, "0");
}

void RedisMgr::DelCount(std::string server_name)
{
    auto lock_key = LOCK_COUNT;
    auto identifier = acquireLock(LOCK_COUNT, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
    Defer defer([this, identifier, lock_key]() { releaseLock(lock_key, identifier); });
    
    HDel(LOGIN_COUNT, server_name);
}

} // namespace core