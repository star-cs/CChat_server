/*
 * @Author: star-cs
 * @Date: 2025-06-14 16:07:53
 * @LastEditTime: 2025-06-14 16:41:32
 * @FilePath: /CChat_server/StatusServer/src/status_service_impl.h
 * @Description: statusService gRPC 服务端
 */
#pragma once
#include "common.h"
#include <grpcpp/grpcpp.h>
#include "status_service.grpc.pb.h"

namespace core
{
    using grpc::Server;
    using grpc::ServerBuilder;
    using grpc::ServerContext;
    using grpc::Status;

    using status::StatusService;

    using status::GetChatServerReq;
    using status::GetChatServerRsp;

    using status::LoginReq;
    using status::LoginRsp;

    class ChatServer
    {
    public:
        ChatServer() : host(""), port(""), name(""), con_count(1) {}
        ChatServer(const ChatServer &cs) : host(cs.host), port(cs.port), name(cs.name), con_count(cs.con_count) {}

        ChatServer &operator=(const ChatServer &cs)
        {
            if (&cs == this)
            {
                return *this;
            }

            host = cs.host;
            name = cs.name;
            port = cs.port;
            con_count = cs.con_count;
            return *this;
        }

        std::string host;
        std::string port;
        std::string name;
        int con_count;
    };

    class StatusServiceImpl final : public StatusService::Service
    {
    public:
        StatusServiceImpl();
        Status GetChatServer(ServerContext *context, const GetChatServerReq *request, GetChatServerRsp *response) override;
        Status Login(ServerContext *context, const LoginReq *request, LoginRsp *response) override;

    private:
        void insertToken(int uid, std::string token);
        ChatServer getChatServer();
        std::unordered_map<std::string, ChatServer> _servers;
        std::mutex _server_mtx;
    };
}