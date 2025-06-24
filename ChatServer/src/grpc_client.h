/*
 * @Author: star-cs
 * @Date: 2025-06-16 11:02:39
 * @LastEditTime: 2025-06-24 10:08:56
 * @FilePath: /CChat_server/ChatServer/src/grpc_client.h
 * @Description:
 */
#pragma once

#include <grpcpp/grpcpp.h>
#include "common.h"
#include "singleton.h"

#include "status_service.grpc.pb.h"
#include "chat_service.grpc.pb.h"

#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

namespace core
{
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using status::LoginReq;
using status::LoginRsp;
using status::StatusService;

using chat::ChatService;
using chat::AddFriendReq;
using chat::AddFriendRsp;
using chat::RplyFriendReq;
using chat::RplyFriendRsp;
using chat::SendChatMsgReq;
using chat::SendChatMsgRsp;
using chat::AuthFriendReq;
using chat::AuthFriendRsp;
using chat::TextChatMsgReq;
using chat::TextChatData;
using chat::TextChatMsgRsp;

template <typename ServiceType>
class RpcConnPool
{
public:
    RpcConnPool(std::size_t poolSize, std::string host, std::string port)
        : _poolSize(poolSize), _host(host), _port(port)
    {
        for (std::size_t i = 0; i < _poolSize; ++i) {
            std::shared_ptr<Channel> channel =
                grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials());

            _connections.push(ServiceType::NewStub(channel));
        }
    }

    std::unique_ptr<typename ServiceType::Stub> getConnection()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock, [this]() {
            if (_b_stop) {
                return true;
            }
            return !_connections.empty();
        });

        if (_b_stop) {
            return nullptr;
        }

        auto context = std::move(_connections.front());
        _connections.pop();
        return context;
    }

    void returnConnection(std::unique_ptr<typename ServiceType::Stub> context)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_b_stop) {
            return;
        }
        _connections.push(std::move(context));
        _cond.notify_one();
    }

    ~RpcConnPool()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _b_stop = true;
        _cond.notify_all();
        while (!_connections.empty()) {
            _connections.pop();
        }
    }

private:
    std::atomic<bool> _b_stop;
    std::size_t _poolSize;
    std::string _host;
    std::string _port;
    std::queue<std::unique_ptr<typename ServiceType::Stub>> _connections;
    std::mutex _mutex;
    std::condition_variable _cond;
};

// gRPC Server 是 StatusServer
// 当前是 ChatServer，只需要 Login
class StatusGrpcClient : public Singleton<StatusGrpcClient>
{
    friend Singleton<StatusGrpcClient>;

public:
    // GetChatServerRsp GetChatServer(int uid);
    LoginRsp Login(int uid, std::string token);

private:
    StatusGrpcClient();
    std::unique_ptr<RpcConnPool<StatusService>> _pool;
};

// ChatServer 服务器之间互为 grpc的服务端和客户端
class ChatGrpcClient : public Singleton<ChatGrpcClient>
{
    friend Singleton<ChatGrpcClient>;

public:
    // 通知添加好友
    // server_ip 发送的对端 chatserver name
    AddFriendRsp NotifyAddFriend(std::string server_ip, const AddFriendReq &req);
    // 通知 统一对方的好友申请
    AuthFriendRsp NotifyAuthFriend(std::string server_ip, const AuthFriendReq &req);
    //
    bool GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo);
    // 发消息
    TextChatMsgRsp NotifyTextChatMsg(std::string server_ip, const TextChatMsgReq &req,
                                     const Json::Value &rtvalue);

private:
    ChatGrpcClient();
    std::unordered_map<std::string, std::unique_ptr<RpcConnPool<ChatService>>> _pools;
};

} // namespace core
