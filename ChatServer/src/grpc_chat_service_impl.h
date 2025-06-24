/*
 * @Author: star-cs
 * @Date: 2025-06-20 21:10:30
 * @LastEditTime: 2025-06-23 20:00:45
 * @FilePath: /CChat_server/ChatServer/src/grpc_chat_service_impl.h
 * @Description: ChatService gRPC server实现类
 */
#pragma once
#include <grpcpp/grpcpp.h>
#include "chat_service.grpc.pb.h"
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
using chat::RplyFriendReq;
using chat::RplyFriendRsp;
using chat::SendChatMsgReq;
using chat::SendChatMsgRsp;
using chat::AuthFriendReq;
using chat::AuthFriendRsp;
using chat::TextChatMsgReq;
using chat::TextChatData;
using chat::TextChatMsgRsp;

class ChatServiceImpl final : public ChatService::Service
{
public:
    ChatServiceImpl();
    Status NotifyAddFriend(ServerContext *context, const AddFriendReq *request,
                           AddFriendRsp *reply) override;
    Status NotifyAuthFriend(ServerContext *context, const AuthFriendReq *request,
                            AuthFriendRsp *response) override;
    Status NotifyTextChatMsg(::grpc::ServerContext *context, const TextChatMsgReq *request,
                             TextChatMsgRsp *response) override;
    bool GetBaseInfo(int uid, std::shared_ptr<UserInfo> &userinfo);
};
} // namespace core
