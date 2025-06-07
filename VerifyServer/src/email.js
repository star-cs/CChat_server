/*
 * @Author: star-cs
 * @Date: 2025-06-07 16:18:10
 * @LastEditTime: 2025-06-07 18:37:45
 * @FilePath: /CChat_server/VerifyServer/src/email.js
 * @Description: 封装发送邮件的模块
 */

const nodemailer = require('nodemailer')    // nodejs邮箱发送库，支持smtp，sendmail等
const config_module = require("./config")


let transport = nodemailer.createTransport({
    host: config_module.email_host,
    port: config_module.email_port,          // SSL 加密端口 465
    secure: config_module.email_secure,       // SSL 加密
    auth: {
        user: config_module.email_user,
        pass: config_module.email_pass
    }
})

/**
 * 发送邮件的函数
 * 
 * transport.sendMail 相当于异步函数，调用的结果通过回调函数通知。
 * 没法同步使用，需要Promise封装调用，抛出Promise给外部，外部通过 await 处理
 * 
 * Promise 封装，将异步操作包装成 Promise，支持 async/await 调用
 * 
 * @param {*} mailOptions_ 邮件配置对象（需包含收件人、主题、内容等）
 * @returns 
 */
function SendMail(mailOptions_) {
    return new Promise(function (reslove, reject) {
        transport.sendMail(mailOptions_, function (error, info) {
            if (error) {
                console.log(error);
                reject(error);
            } else {
                console.log('邮件已成功发送：' + info.response);
                reslove(info.response)
            }
        })
    })
}


/**
 * 给 module.exports对象添加一个属性
 * const mailModule = require('./mailModule');
 * mailModule.SendMail(); // 调用SendMail函数
 */
module.exports.SendMail = SendMail