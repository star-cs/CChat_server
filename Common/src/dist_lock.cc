/*
 * @Author: star-cs
 * @Date: 2025-06-30 11:24:36
 * @LastEditTime: 2025-06-30 19:25:30
 * @FilePath: /CChat_server/Common/src/dist_lock.cc
 * @Description: 
 */
#include "dist_lock.h"
#include "common.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <cstddef>
#include <hiredis/hiredis.h>
#include <hiredis/read.h>
#include <string>
#include <thread>

namespace core
{

static std::string generateUUID()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

std::string DistLock::acquireLock(redisContext *context, const std::string &lockName,
                                  int lockTimeout, int acquireTimeout)
{
    std::string identifier = generateUUID();
    std::string lockKey = "lock:" + lockName;
    auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(acquireTimeout);

    while (std::chrono::steady_clock::now() < endTime) {
        // 使用 SET 命令尝试加锁：SET lockKey identifier NX EX lockTimeout
        redisReply *reply = (redisReply *)redisCommand(
            context, "SET %s %s NX EX %d", lockKey.c_str(), identifier.c_str(), lockTimeout);
        if (reply != nullptr) {
            // 判断返回结果是否为 OK
            if (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK") {
                freeReplyObject(reply);
                return identifier;
            }
            freeReplyObject(reply);
        }
        // 暂停 1 毫秒后重试，防止忙等待
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return "";
}

bool DistLock::releaseLock(redisContext *context, const std::string &lockName,
                           const std::string &identifier)
{
    std::string lockKey = "lock:" + lockName;
    // Lua 脚本：判断锁标识是否匹配，匹配则删除锁
    const char *luaScript = "if redis.call('get', KEYS[1]) == ARGV[1] then \
                                return redis.call('del', KEYS[1]) \
                             else \
                                return 0 \
                             end";
    // 调用 EVAL 命令执行 Lua 脚本，第一个参数为脚本，后面依次为 key 的数量、key 以及对应的参数
    redisReply *reply = (redisReply *)redisCommand(context, "EVAL %s 1 %s %s", luaScript,
                                                   lockKey.c_str(), identifier.c_str());
    bool success = false;
    if (reply != nullptr) {
        // 当返回整数值为 1 时，表示成功删除了锁
        if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1) {
            success = true;
        }
        freeReplyObject(reply);
    }
    return success;
}

} // namespace core