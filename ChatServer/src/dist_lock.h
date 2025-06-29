/*
 * @Author: star-cs
 * @Date: 2025-06-28 16:27:27
 * @LastEditTime: 2025-06-28 16:41:00
 * @FilePath: /CChat_server/ChatServer/src/dist_lock.h
 * @Description: 分布式锁封装
 */
#pragma once

#include <hiredis/hiredis.h>
#include <string>

namespace core
{

class DistLock
{
public:
    static DistLock &GetInstance()
    {
        static DistLock lock;
        return lock;
    }

    ~DistLock() = default;

    /**
    尝试获取锁，返回锁的唯一标识符（UUID），如果获取失败则返回空字符串
    acquireTimeout时间内，不断尝试加锁
    */
    std::string acquireLock(redisContext *context, const std::string &lockName, int lockTimeout,
                            int acquireTimeout);

    /**
    释放锁，只有锁的持有者才能释放，返回是否成功
                   
    */
    bool releaseLock(redisContext *context, const std::string &lockName,
                     const std::string &identifier);

private:
    DistLock() = default;
};

} // namespace core