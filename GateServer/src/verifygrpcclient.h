/*
 * @Author: star-cs
 * @Date: 2025-06-06 20:53:47
 * @LastEditTime: 2025-06-07 22:20:41
 * @FilePath: /CChat_server/GateServer/src/verifygrpcclient.h
 * @Description: 申请验证码服务器， rpc 客户端
 */
#pragma once

#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"
#include "common.h"
#include "singleton.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

namespace core
{
    class RpcConnPool
    {
    public:
        RpcConnPool(size_t poolSize, std::string host, std::string port);
        ~RpcConnPool();
        std::unique_ptr<VerifyService::Stub> getConnection();
        void returnConnection(std::unique_ptr<VerifyService::Stub> context);
        void Close();

    private:
        std::atomic<bool> _b_stop;
        size_t _poolSize;
        std::string _host;
        std::string _port;
        std::queue<std::unique_ptr<VerifyService::Stub>> _connections;
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
        std::unique_ptr<RpcConnPool> _pool;
    };
}
