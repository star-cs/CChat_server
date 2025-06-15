/*
 * @Author: star-cs
 * @Date: 2025-06-06 09:55:24
 * @LastEditTime: 2025-06-14 17:47:34
 * @FilePath: /CChat_server/GateServer/src/logicsystem.cc
 * @Description:
 */
#include "logicsystem.h"
#include "httpconnection.h"
#include "grpc_client.h"
#include "redis_mgr.h"
#include "mysql_mgr.h"

#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>

namespace core
{
    /**
     * Get
     * /get_test
     *
     *
     *
     * POST
     * /get_verifycode
     *
     *
     *
     */
    LogicSystem::LogicSystem()
    {
        RegGet("/get_test", [](std::shared_ptr<HttpConnection> connection)
               {
                   beast::ostream(connection->_response.body()) << "receive get_test req " << std::endl;
                   for (auto &elem : connection->_get_params)
                   {
                       beast::ostream(connection->_response.body()) << "key=" << elem.first << " value=" << elem.second << "\n";
                   } });

        RegPost("/get_verifycode", [](std::shared_ptr<HttpConnection> connection)
                {
                    auto body_str = beast::buffers_to_string(connection->_request.body().data());
                    std::cout << "receive body is " << body_str << std::endl;
                    connection->_response.set(http::field::content_type, "text/json");
                    Json::Value response_root;
                    Json::Reader reader;
                    Json::Value ori_root;
                    bool parser_success = reader.parse(body_str, ori_root);
                    if (!parser_success)
                    {
                        std::cout << "Failed to parse JSON data!" << std::endl;
                        response_root["error"] = ErrorCodes::Error_Json;
                        std::string jsonstr = response_root.toStyledString();
                        beast::ostream(connection->_response.body()) << jsonstr;
                        return true;
                    }

                    if (!ori_root.isMember("email"))
                    { // 没有email
                        std::cout << "Failed to parse JSON data!" << std::endl;
                        response_root["error"] = ErrorCodes::Error_Json;
                        std::string jsonstr = response_root.toStyledString();
                        beast::ostream(connection->_response.body()) << jsonstr;
                        return true;
                    }

                    auto email = ori_root["email"].asString();
                    GetVerifyRsp rsp = VerifyGrpcClient::GetInstance()->GetVerifyCode(email);
                    std::cout << "email is " << email << std::endl;
                    response_root["error"] = rsp.error();
                    response_root["email"] = ori_root["email"];
                    std::string jsonstr = response_root.toStyledString();
                    beast::ostream(connection->_response.body()) << jsonstr;
                    return true; });

        RegPost("/register", [](std::shared_ptr<HttpConnection> connection)
                {
                    auto body_str = beast::buffers_to_string(connection->_request.body().data());
                    LOG_INFO("receive body is {} ",body_str);
                    connection->_response.set(http::field::content_type, "text/json");
                    Json::Value ori_json;
                    Json::Reader reader;
                    Json::Value resp_json;
                    auto parser_success = reader.parse(body_str, ori_json);
                    if (!parser_success)
                    {
                        std::cout << "Json reder error" << std::endl;
                        std::cout << "Failed to parse JSON data!" << std::endl;
                        resp_json["error"] = ErrorCodes::Error_Json;
                        std::string respstring = resp_json.toStyledString();
                        beast::ostream(connection->_response.body()) << respstring;
                        return true;
                    }

                    // qt会校验，保证发送过来的是有数据的。
                    const std::string email = ori_json["email"].asString();
                    const std::string name = ori_json["name"].asString();
                    const std::string pwd = ori_json["passwd"].asString();
                    const std::string confirm = ori_json["confirm"].asString();
                    const std::string icon = ori_json["icon"].asString();

                    if (pwd != confirm)
                    {
                        std::cout << "password err " << std::endl;
                        resp_json["error"] = ErrorCodes::PasswdErr;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }

                    // 判断验证码
                    std::string verfiy_code;
                    bool b_get_verify = RedisMgr::GetInstance()->Get(CODE_PREFIX + email, verfiy_code);
                    if (!b_get_verify)
                    {
                        std::cout << " get verify code expired" << std::endl;
                        resp_json["error"] = ErrorCodes::VerifyExpired;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }

                    if (verfiy_code != ori_json["verifycode"].asString())
                    {
                        std::cout << " verify code error" << std::endl;
                        resp_json["error"] = ErrorCodes::VerifyCodeErr;
                        std::string jsonstr = resp_json.toStyledString();
                        beast::ostream(connection->_response.body()) << jsonstr;
                        return true;
                    }

                    // 数据库 操作
                    int uid = MysqlMgr::GetInstance()->RegUser(name, email, pwd, icon);
                    if (uid == 0 || uid == -1) {
                        std::cout << " user or email exist" << std::endl;
                        resp_json["error"] = ErrorCodes::UserExist;
                        beast::ostream(connection->_response.body()) <<  resp_json.toStyledString();
                        return true;
                    }
                    resp_json["error"] = ErrorCodes::Success;
                    resp_json["uid"] = uid;
                    resp_json["email"] = email;
                    resp_json["name"] = name;
                    resp_json["pwd"] = pwd;
                    resp_json["confirm"] = confirm;
                    resp_json["icon"] = icon;
                    resp_json["verifycode"] = ori_json["verifycode"].asString();
                    beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                    return true; });

        RegPost("/reset_pwd", [](std::shared_ptr<HttpConnection> connection)
                {
                    auto body_str = beast::buffers_to_string(connection->_request.body().data());
                    LOG_INFO("receive body is {} ", body_str);
                    connection->_response.set(http::field::content_type, "text/json");
                    Json::Value resp_json;
                    Json::Reader reader;
                    Json::Value ori_json;
                    auto parser_success = reader.parse(body_str, ori_json);
                    if (!parser_success)
                    {
                        LOG_ERROR("Json error, Failed to parser JSON data!");
                        resp_json["error"] = ErrorCodes::Error_Json;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }

                    const std::string name = ori_json["name"].asString();
                    const std::string email = ori_json["email"].asString();
                    const std::string passwd = ori_json["passwd"].asString();
                    const std::string varify_code = ori_json["verifycode"].asString();

                    // 检查 email redis 验证码
                    std::string  true_varify_code;
                    bool b_get_varify = RedisMgr::GetInstance()->Get(CODE_PREFIX + email, true_varify_code);
                    if(!b_get_varify){
                        LOG_ERROR("get varify code expired");
                        resp_json["error"] = ErrorCodes::VerifyExpired;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }

                    if(true_varify_code != varify_code)
                    {
                        LOG_ERROR("varfiy code erro");
                        resp_json["error"] = ErrorCodes::VerifyCodeErr;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }

                    // 查询数据库，邮箱和用户名是否匹配
                    bool b_email_vaild = MysqlMgr::GetInstance()->CheckEmail(name, email);
                    if(!b_email_vaild){
                        LOG_ERROR("name:{}, email:{} is not exist", name, email);
                        resp_json["error"] = ErrorCodes::EmailNotMatch;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }
                    
                    // 修改 密码
                    bool b_up = MysqlMgr::GetInstance()->UpdatePwd(email, passwd);
                    if(!b_up){
                        LOG_ERROR("email:{}, update pwd failed", email);
                        resp_json["error"] = ErrorCodes::PasswdUpFailed;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }

                    LOG_INFO("succeed to update password: {}", passwd);
                    resp_json["error"] = ErrorCodes::Success;
                    resp_json["email"] = email;
                    resp_json["name"] = name;
                    resp_json["passwd"] = passwd;
                    resp_json["verifycode"] = varify_code;
                    beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                    return true; });

        RegPost("/login", [](std::shared_ptr<HttpConnection> connection)
                {
                    auto body_str = beast::buffers_to_string(connection->_request.body().data());
                    LOG_INFO("receive body is {} ", body_str);
                    connection->_response.set(http::field::content_type, "text/json");
                    Json::Value resp_json;
                    Json::Reader reader;
                    Json::Value ori_json;
                    auto parser_success = reader.parse(body_str, ori_json);
                    if (!parser_success)
                    {
                        LOG_ERROR("Json error, Failed to parser JSON data!");
                        resp_json["error"] = ErrorCodes::Error_Json;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }

                    const std::string email = ori_json["email"].asString();
                    const std::string passwd = ori_json["passwd"].asString();

                    // 1. 查看数据库 判断 邮箱和密码 是否匹配
                    UserInfo userInfo;
                    bool b_pwd_vaild = MysqlMgr::GetInstance()->CheckPwd(email, passwd, userInfo);
                    if (!b_pwd_vaild)
                    {
                        LOG_ERROR("user pwd not match");
                        resp_json["error"] = ErrorCodes::PasswdInvalid;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }

                    // 2. 查询 StatusServer找到合适的连接
                    auto reply = StatusGrpcClient::GetInstance()->GetChatServer(userInfo.uid);
                    if (reply.error())
                    {
                        LOG_ERROR("grpc get chat server failed, error is {}", reply.error());
                        resp_json["error"] = ErrorCodes::RPCFailed;
                        beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                        return true;
                    }

                    LOG_INFO("succeed to load userinfo uid is {}", userInfo.uid);
                    resp_json["error"] = ErrorCodes::Success;
                    resp_json["email"] = email;
                    resp_json["uid"] = userInfo.uid;
                    resp_json["token"] = reply.token();
                    resp_json["host"] = reply.host();
                    resp_json["port"] = reply.port();
                    beast::ostream(connection->_response.body()) << resp_json.toStyledString();
                    return true;
                });
    }

    LogicSystem::~LogicSystem()
    {
    }

    bool LogicSystem::HandleGet(const std::string &str, std::shared_ptr<HttpConnection> conn)
    {
        if (_get_handlers.find(str) == _get_handlers.end())
        {
            return false;
        }
        _get_handlers[str](conn);
        return true;
    }

    void LogicSystem::RegGet(const std::string &str, HttpHandler handler)
    {
        _get_handlers.insert(make_pair(str, handler));
    }

    bool LogicSystem::HandlePost(const std::string &str, std::shared_ptr<HttpConnection> conn)
    {
        if (_post_handlers.find(str) == _post_handlers.end())
        {
            return false;
        }
        _post_handlers[str](conn);
        return true;
    }

    void LogicSystem::RegPost(const std::string &str, HttpHandler handler)
    {
        _post_handlers.insert(make_pair(str, handler));
    }

}