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
    class VerifyGrpcClient : public Singleton<VerifyGrpcClient>
    {
        friend class Singleton<VerifyGrpcClient>;

    public:
        GetVerifyRsp GetVerifyCode(std::string email);

    private:
        VerifyGrpcClient();
        std::unique_ptr<VerifyService::Stub> stub_;     // gRPC 存根
    };
}
