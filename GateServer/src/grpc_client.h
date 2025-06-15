/*
 * @Author: star-cs
 * @Date: 2025-06-06 20:53:47
 * @LastEditTime: 2025-06-14 16:39:32
 * @FilePath: /CChat_server/GateServer/src/grpc_client.h
 * @Description: 申请验证码服务器， rpc 客户端
 */
#pragma once

#include <grpcpp/grpcpp.h>
#include "verify_service.grpc.pb.h"
#include "status_service.grpc.pb.h"
#include "common.h"
#include "singleton.h"

namespace core
{

    using grpc::Channel;
    using grpc::ClientContext;
    using grpc::Status;

    using verify::GetVerifyReq;
    using verify::GetVerifyRsp;
    using verify::VerifyService;

    using status::GetChatServerReq;
    using status::GetChatServerRsp;
    using status::LoginReq;
    using status::LoginRsp;
    using status::StatusService;
    
    template <typename ServiceType>
    class RpcConnPool
    {
    public:
        RpcConnPool(std::size_t poolSize, std::string host, std::string port)
            : _b_stop(false), _poolSize(poolSize), _host(host), _port(port)
        {
            for (std::size_t i = 0; i < _poolSize; ++i)
            {
                std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials()); // gRPC channel

                _connections.emplace(ServiceType::NewStub(channel));
            }
        }
        ~RpcConnPool()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            Close();
            while (!_connections.empty())
            {
                _connections.pop();
            }
        }
        std::unique_ptr<typename ServiceType::Stub> getConnection()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [this]()
                       {
            if(_b_stop) return true;
            return !_connections.empty(); });

            auto context = std::move(_connections.front());
            _connections.pop();
            return context;
        }
        void returnConnection(std::unique_ptr<typename ServiceType::Stub> context)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_b_stop)
            {
                return;
            }
            _connections.push(std::move(context));
            _cond.notify_one();
        }
        void Close()
        {
            _b_stop = true;
            _cond.notify_all();
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

    class VerifyGrpcClient : public Singleton<VerifyGrpcClient>
    {
        friend class Singleton<VerifyGrpcClient>;

    public:
        GetVerifyRsp GetVerifyCode(std::string email);

    private:
        VerifyGrpcClient();
        std::unique_ptr<RpcConnPool<VerifyService>> _pool;
    };

    class StatusGrpcClient : public Singleton<StatusGrpcClient>
    {
        friend class Singleton<StatusGrpcClient>;

    public:
        GetChatServerRsp GetChatServer(int uid);
        LoginRsp Login(int uid, std::string token);

    private:
        StatusGrpcClient();
        std::unique_ptr<RpcConnPool<StatusService>> _pool;
    };

}
