/*
 * @Author: star-cs
 * @Date: 2025-06-20 21:10:30
 * @LastEditTime: 2025-07-07 22:58:29
 * @FilePath: /CChat_server/ChatServer/src/grpc_chat_service_impl.h
 * @Description: ChatService gRPC server实现类
 */
#pragma once
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/server_context.h>
#include <memory>
#include "chat_service.grpc.pb.h"
#include "chat_service.pb.h"
#include "common.h"

namespace core
{
using grpc::Server;
using grpc::ServerBuilder;
using grpc::Status;
using grpc::ServerContext;

using chat::ChatService;
using chat::AddFriendReq;
using chat::AddFriendRsp;
using chat::AddFriendMsg;
using chat::AuthFriendReq;
using chat::AuthFriendRsp;
using chat::TextChatMsgReq;
using chat::TextChatData;
using chat::TextChatMsgRsp;
using chat::KickUserReq;
using chat::KickUserRsp;

class CServer;
class ChatServiceImpl final : public ChatService::Service
{
public:
    ChatServiceImpl();
    Status NotifyAddFriend(ServerContext *context, const AddFriendReq *request,
                           AddFriendRsp *reply) override;
    Status NotifyAuthFriend(ServerContext *context, const AuthFriendReq *request,
                            AuthFriendRsp *response) override;
    Status NotifyTextChatMsg(ServerContext *context, const TextChatMsgReq *request,
                             TextChatMsgRsp *response) override;
    bool GetBaseInfo(int uid, std::shared_ptr<UserInfo> &userinfo);

    Status NotifyKickUser(ServerContext *context, const KickUserReq *request,
                          KickUserRsp *response) override;

    void RegisterServer(std::shared_ptr<CServer> pServer);

private:
    std::shared_ptr<CServer> _p_server;
};
} // namespace core
