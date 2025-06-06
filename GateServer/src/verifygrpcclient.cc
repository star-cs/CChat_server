#include "verifygrpcclient.h"
#include "configmgr.h"

namespace core
{
    GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email)
    {
        ClientContext context;
        GetVerifyReq request;
        GetVerifyRsp response;
        request.set_email(email);

        // 通过 存根 调用 RPC 方法
        Status status = stub_->GetVerifyCode(&context, request, &response);

        if (!status.ok())
        {
            response.set_error(ErrorCodes::RPCFailed);
        }

        return response;
    }

    VerifyGrpcClient::VerifyGrpcClient()
    {
        auto &config_mgr = ConfigMgr::GetInstance();
        std::string host = config_mgr["VarifyServer"]["host"];
        std::string port = config_mgr["VarifyServer"]["port"];
        std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials()); // gRPC channel
        stub_ = VerifyService::NewStub(channel);                                                                       // 服务接口的 NewStub方法去创建存根
    }

}