/*
 * @Author: star-cs
 * @Date: 2025-06-16 11:22:03
 * @LastEditTime: 2025-06-24 10:15:10
 * @FilePath: /CChat_server/ChatServer/src/grpc_client.cc
 * @Description:
 */
#include "grpc_client.h"

#include "common.h"
#include "configmgr.h"
#include <cstddef>

namespace core
{

StatusGrpcClient::StatusGrpcClient()
{
    auto &cfg = ConfigMgr::GetInstance();
    std::size_t poolSize = boost::lexical_cast<std::size_t>(cfg["RpcConnPool"]["StatusServer"]);
    std::string host = cfg["StatusServer"]["host"];
    std::string port = cfg["StatusServer"]["port"];
    _pool.reset(new RpcConnPool<StatusService>(poolSize, host, port));
    LOG_INFO("StatusGrpcClient sucess, poolsize:{}, hose:{}, port:{}", poolSize, host, port);
}

LoginRsp StatusGrpcClient::Login(int uid, std::string token)
{
    ClientContext context;
    LoginReq request;
    LoginRsp response;

    request.set_uid(uid);
    request.set_token(token);
    auto stub = _pool->getConnection();

    Status status = stub->Login(&context, request, &response);

    Defer defer([&stub, this]() { _pool->returnConnection(std::move(stub)); });

    if (status.ok()) {
        return response;
    } else {
        response.set_error(ErrorCodes::RPCFailed);
        return response;
    }
}

ChatGrpcClient::ChatGrpcClient()
{
    auto &cfg = ConfigMgr::GetInstance();
    std::string serverList = cfg["PeerServer"]["Servers"];

    std::stringstream ss(serverList);
    std::vector<std::string> words;
    std::string word;
    while (std::getline(ss, word, ',')) {
        words.push_back(word);
    }

    for (auto &word : words) {
        if (cfg[word]["name"].empty() || cfg[word]["name"] != word) {
            continue;
        }
        int num = boost::lexical_cast<int>(cfg["RpcConnPool"]["PeerServer"]);
        std::string host = cfg[word]["host"];
        std::string port = cfg[word]["port"];
        _pools[word] =
            std::make_unique<RpcConnPool<ChatService>>(num, host, port);
        LOG_INFO("ChatGrpcClient sucess, perrserver:{}, poolsize:{}, hose:{}, port:{}", word, num, host, port);
    }
}

// 通知添加好友
AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string server_ip, const AddFriendReq &req)
{
    AddFriendRsp rsp;
    Defer defer([&rsp, &req]() {
        rsp.set_error(ErrorCodes::Success);
        rsp.set_applyuid(req.applyuid());
        rsp.set_touid(req.touid());
    });
    // 找到目标服务器的rpc连接，发送~
    auto iter = _pools.find(server_ip);
    if (iter == _pools.end()) {
        LOG_ERROR("rpc client 连接没找到，server_ip:{}", server_ip);
        return rsp;
    }
    auto &pool = iter->second;
    ClientContext ctx;
    auto stub = pool->getConnection();
    if (stub == nullptr) {
        LOG_ERROR("rpc client stub没法使用");
        return rsp;
    }

    Defer defer_conn([&stub, this, &pool]() { pool->returnConnection(std::move(stub)); });

    Status status = stub->NotifyAddFriend(&ctx, req, &rsp);

    if (!status.ok()) {
        rsp.set_error(ErrorCodes::RPCFailed);
    }
    return rsp;
}

// 通知 同意 对方的好友申请
AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string server_ip, const AuthFriendReq &req)
{
    AuthFriendRsp rsp;
    rsp.set_error(ErrorCodes::Success);

    Defer defer([&rsp, &req]() {
        rsp.set_fromuid(req.fromuid());
        rsp.set_touid(req.touid());
    });

    auto iter = _pools.find(server_ip);
    if (iter == _pools.end()) {
        LOG_ERROR("rpc client 连接没找到，server_ip:{}", server_ip);
        return rsp;
    }

    auto &pool = iter->second;
    ClientContext ctx;
    auto stub = pool->getConnection();
    if (stub == nullptr) {
        LOG_ERROR("rpc client stub没法使用");
        return rsp;
    }
    Status status = stub->NotifyAuthFriend(&ctx, req, &rsp);

    Defer defercon([&stub, this, &pool]() { pool->returnConnection(std::move(stub)); });

    if (!status.ok()) {
        rsp.set_error(ErrorCodes::RPCFailed);
        return rsp;
    }
    return rsp;
}

//
bool ChatGrpcClient::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo)
{
    return true;
}

// 发消息
TextChatMsgRsp ChatGrpcClient::NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq &req,
                                                 const Json::Value &rtvalue)
{
    TextChatMsgRsp rsp;
    return rsp;
}

} // namespace core