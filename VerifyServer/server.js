/*
 * @Author: star-cs
 * @Date: 2025-06-07 15:56:27
 * @LastEditTime: 2025-06-10 22:13:26
 * @FilePath: /CChat_server/VerifyServer/server.js
 * @Description: 验证服务器 主程序
 */

// {v4: uuidv4} 表示 从 'uuid' 包中取出 v4 方法并重命名为 uudiv4
const { v4: uuidv4 } = require('uuid');

const grpc = require('@grpc/grpc-js')
const common_modeuls = require('./src/common')
const config_module = require('./src/config')
const email_module = require('./src/email')
const message_proto = require('./src/proto')
const redis_module = require('./src/redis')

/**
 * 异步处理：1. async 声明异步函数；2. await 等待异步操作完成；
 * @param {*} call 客户端请求数据
 * @param {*} callback 向客户端响应的函数
 */
async function GetVerifyCode(call, callback) {
    console.log("email is ", call.request.email)
    try {
        let query_res = await redis_module.GetRedis(common_modeuls.code_prefix + call.request.email);
        console.log("query_res is ", query_res)
        
        let uniqueId = query_res;   // 如果 email 已经申请过，直接复用。
        if (query_res == null) {
            uniqueId = uuidv4().replace(/\D/g, '');
            uniqueId = uniqueId.substring(0, 4);

            let res = await redis_module.SetRedisExpire(
                common_modeuls.code_prefix + call.request.email,
                uniqueId, 3 * 60
            );
            if (!res) {
                callback(null, {
                    email: call.request.email,
                    error: common_modeuls.Errors.RedisErr
                })
                return;
            }
        }
        console.log("uniqueId is ", uniqueId)
        let text_str = '您的验证码为' + uniqueId + '请在三分钟之内完成注册'
        // 发件人，收件人，主题和内容
        let mailOptions = {
            from: config_module.email_user,
            to: call.request.email,
            subject: '验证码',
            text: text_str,
            html: `
                <div style="font-family: Arial, sans-serif; padding: 20px; color: #333;">
                <h2 style="color: #4CAF50;">您好，感谢您的注册！</h2>
                <p style="font-size: 16px;">您的验证码为：</p>
                <div style="font-size: 24px; font-weight: bold; color: #E91E63; margin: 10px 0;">
                    ${uniqueId}
                </div>
                <p style="font-size: 14px; color: #888;">请在 <strong>3 分钟内</strong>完成注册。</p>
                <hr style="border: none; border-top: 1px solid #eee; margin: 20px 0;">
                <p style="font-size: 12px; color: #999;">如果您没有申请该验证码，请忽略此邮件。</p>
                </div>
            `
        };
        // 添加 await 等待 Promise 的 SendMail 执行完毕
        let send_res = await email_module.SendMail(mailOptions);
        console.log("send res is ", send_res)

        // 第一个参数：错误对象（null表示无错误）
        // 第二个参数：成功响应对象
        callback(null, {
            email: call.request.email,
            error: common_modeuls.Errors.Success
        })

    } catch (error) {
        console.log("catch error is ", error)
        callback(null, {
            email: call.request.email,
            error: common_modeuls.Errors.Exception
        })
    }
}


function main() {
    // 新建一个 gRPC 服务器对象
    var server = new grpc.Server()
    // 注册服务定义
    // 根据 proto 文件生成的 gRPC 服务定义
    // 
    // {GetVerifyCode: GetVerifyCode} 
    // 具体实现，key 是 proto 定义的方法名，value 是你写的js方法
    server.addService(message_proto.VerifyService.service, {
        GetVerifyCode: GetVerifyCode
    })
    let server_path = config_module.VerifyServer_host + ':' + config_module.VerifyServer_port;

    // server.bindAsync() 绑定监听端口
    // grpc.ServerCredentials.createInsecure() 不加密服务器凭据
    // () => {...} 绑定完成后执行的回调函数
    server.bindAsync(server_path,
        grpc.ServerCredentials.createInsecure(),
        () => {
            // server.start();
            console.log('grpc server started')
            console.log('VerifyServer: ' + server_path)
        }
    )
}

main()