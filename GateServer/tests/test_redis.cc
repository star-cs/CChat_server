/*
 * @Author: star-cs
 * @Date: 2025-06-08 11:49:22
 * @LastEditTime: 2025-06-08 14:14:54
 * @FilePath: /CChat_server/GateServer/tests/test_redis.cc
 * @Description: 测试 Redis
 */

#include "hiredis/hiredis.h"
#include <cstring>
#include <string>

void test_redis()
{
    // 连接redis 需要启动才可以进行连接
    // redis默认监听端口为6387 可以再配置文件中修改
    redisContext *c = redisConnect("81.68.86.146", 6380);
    if (c->err)
    {
        printf("Connect to redisServer faile:%s\n", c->errstr);
        redisFree(c);
        return;
    }
    printf("Connect to redisServer Success\n");
    std::string redis_password = "123456";
    redisReply *r = (redisReply *)redisCommand(c, "AUTH %s", redis_password.c_str());
    if (r->type == REDIS_REPLY_ERROR)
    {
        printf("Redis认证失败！\n");
    }
    else
    {
        printf("Redis认证成功！\n");
    }
    // 为redis设置key
    const char *command1 = "set stest1 value1";
    // 执行redis命令行
    r = (redisReply *)redisCommand(c, command1);
    // 如果返回NULL则说明执行失败
    if (NULL == r)
    {
        printf("Execut command1 failure\n");
        redisFree(c);
        return;
    }
    // 如果执行失败则释放连接
    if (!(r->type == REDIS_REPLY_STATUS && (strcmp(r->str, "OK") == 0 || strcmp(r->str, "ok") == 0)))
    {
        printf("Failed to execute command[%s]\n", command1);
        freeReplyObject(r);
        redisFree(c);
        return;
    }
    // 执行成功 释放redisCommand执行后返回的redisReply所占用的内存
    freeReplyObject(r);
    printf("Succeed to execute command[%s]\n", command1);
    const char *command2 = "strlen stest1";
    r = (redisReply *)redisCommand(c, command2);
    // 如果返回类型不是整形 则释放连接
    if (r->type != REDIS_REPLY_INTEGER)
    {
        printf("Failed to execute command[%s]\n", command2);
        freeReplyObject(r);
        redisFree(c);
        return;
    }
    // 获取字符串长度
    int length = r->integer;
    freeReplyObject(r);
    printf("The length of 'stest1' is %d.\n", length);
    printf("Succeed to execute command[%s]\n", command2);
    // 获取redis键值对信息
    const char *command3 = "get stest1";
    r = (redisReply *)redisCommand(c, command3);
    if (r->type != REDIS_REPLY_STRING)
    {
        printf("Failed to execute command[%s]\n", command3);
        freeReplyObject(r);
        redisFree(c);
        return;
    }
    printf("The value of 'stest1' is %s\n", r->str);
    freeReplyObject(r);
    printf("Succeed to execute command[%s]\n", command3);
    const char *command4 = "get stest2";
    r = (redisReply *)redisCommand(c, command4);
    if (r->type != REDIS_REPLY_NIL)
    {
        printf("Failed to execute command[%s]\n", command4);
        freeReplyObject(r);
        redisFree(c);
        return;
    }
    freeReplyObject(r);
    printf("Succeed to execute command[%s]\n", command4);
    // 释放连接资源
    redisFree(c);
}

int main(){
    test_redis();
    return 0;
}