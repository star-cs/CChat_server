/*
 * @Author: star-cs
 * @Date: 2025-06-08 20:01:05
 * @LastEditTime: 2025-06-09 15:13:15
 * @FilePath: /CChat_server/VerifyServer/src/redis.js
 * @Description: redis连接
 * 
 * 
 */

const config_module = require('./config')
const redis = require("ioredis")

const redisCli = new redis.Redis({
    host: config_module.redis_host,
    port: config_module.redis_port,
    password: config_module.redis_passwd
});

// 监听错误信息
redisCli.on("error", function (error) {
    console.log("RedisCli connect error");
    redisCli.quit();
})


/**
 * 从 redis 获取 key 对应的 value
 * @param {*} key 
 * @returns 
 */
async function GetRedis(key) {
    try {
        const value = await redisCli.get(key)
        if (value === null) {
            console.log('result:', '<' + result + '>', 'This key cannot be find...')
            return null
        }
        console.log('Result:', '<' + result + '>', 'Get key success!...');
        return result
    } catch (error) {
        console.log('GetRedis error is', error);
        return null
    }
}

/**
 * 查询key是否存在
 * @param {*} key 
 * @returns 
 */
async function QueryRedis(key) {
    try {
        const result = redisCli.exists(key)
        //  判断该值是否为空 如果为空返回null
        if (result === 0) {
            console.log('result:<', '<' + result + '>', 'This key is null...');
            return null
        }
        console.log('Result:', '<' + result + '>', 'With this value!...');
        return result
    } catch (error) {
        console.log('GetRedis error is', error);
        return null
    }
}


/**
 * 设置 key-value 超时时间
 * @param {*} key 
 * @param {*} value 
 * @param {*} exptime 秒
 */
async function SetRedisExpire(key, value, exptime) {
    try {
        // await redisCli.set(key, value)
        // await redisCli.expire(key, exptime)
        await redisCli.set(key, value, 'EX', exptime);
        return true;
    } catch (error) {
        console.log('SetRedisExpire error is', error);
        return false;
    }
}

/**
 * 退出函数
 */
function Quit(){
    RedisCli.quit();
}

module.exports = {GetRedis, QueryRedis, SetRedisExpire, Quit};

