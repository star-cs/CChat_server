/*
 * @Author: star-cs
 * @Date: 2025-06-07 16:14:38
 * @LastEditTime: 2025-06-07 16:16:01
 * @FilePath: /CChat_server/VerifyServer/src/common.js
 * @Description: 基础配置信息
 */

let code_prefix = "code_"

const Errors = {
    Success: 0,
    RedisErr: 1,
    Exception: 2,
};

module.exports = { code_prefix, Errors }