/*
 * @Author: star-cs
 * @Date: 2025-06-07 16:16:26
 * @LastEditTime: 2025-06-07 18:38:15
 * @FilePath: /CChat_server/VerifyServer/src/config.js
 * @Description: 读取配置信息
 */

const fs = require('fs')

// json 解析
let config = JSON.parse(fs.readFileSync('config.json', 'utf-8'));
let VerifyServer_host = config.VerifyServer.host;
let VerifyServer_port = config.VerifyServer.port;
let email_host = config.email.host;
let email_port = config.email.port;
let email_secure = config.email.secure;
let email_user = config.email.user;
let email_pass = config.email.pass;
let mysql_host = config.mysql.host;
let mysql_port = config.mysql.port;
let mysql_passwd = config.mysql.passwd;
let redis_host = config.redis.host;
let redis_port = config.redis.port;
let redis_passwd = config.redis.passwd;
let code_prefix = "code_";

module.exports = {
    VerifyServer_host, VerifyServer_port,
    email_host, email_port, email_secure,
    email_pass, email_user,
    mysql_host, mysql_port, mysql_passwd,
    redis_host, redis_port, redis_passwd,
    code_prefix
}
