#include "common.h"
#include "singleton.h"
#include "hiredis/hiredis.h"
#include <atomic>

namespace core
{
class RedisConnPool
{
public:
    RedisConnPool(std::size_t pool_size, const char *host, unsigned short port, const char *passwd);

    void checkThread();


    ~RedisConnPool();
    redisContext *getConnection();

    void returnConnection(redisContext *c);

    void Close();

private:
    bool reconnect();


private:
    std::atomic<bool> _b_stop;
    std::size_t _pool_size;
    const char *_host;
    unsigned short _port;
    const char *_pwd;
    std::queue<redisContext *> _conns;
    std::mutex _mutex;
    std::condition_variable _cond;
    std::thread _check_thread;
    std::atomic<int> _fail_count;
};

class RedisMgr : public Singleton<RedisMgr>
{
    friend Singleton<RedisMgr>;

public:
    ~RedisMgr();
    bool Get(const std::string &key, std::string &value);
    bool Set(const std::string &key, const std::string &value);
    bool LPush(const std::string &key, const std::string &value);
    bool LPop(const std::string &key, std::string &value);
    bool RPush(const std::string &key, const std::string &value);
    bool RPop(const std::string &key, std::string &value);
    bool HSet(const std::string &key, const std::string &hkey, const std::string &value);
    bool HSet(const char *key, const char *hkey, const char *hvalue, size_t hvaluelen);
    bool HDel(const std::string &key, const std::string &field);
    std::string HGet(const std::string &key, const std::string &hkey);
    bool Del(const std::string &key);
    bool ExistsKey(const std::string &key);
    void Close() { _redisConnPool->Close(); }

    std::string acquireLock(const std::string &lockName, int lockTimeout, int acquireTimeout);

    bool releaseLock(const std::string &lockName, const std::string &identifier);

    void IncreaseCount(std::string server_name);
    void DecreaseCount(std::string server_name);
    void InitCount(std::string server_name);
    void DelCount(std::string server_name);

private:
    RedisMgr();

private:
    std::unique_ptr<RedisConnPool> _redisConnPool;
};

} // namespace core
