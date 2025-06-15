/*
 * @Author: star-cs
 * @Date: 2025-06-06 20:53:34
 * @LastEditTime: 2025-06-14 15:17:21
 * @FilePath: /CChat_server/GateServer/src/grpc_client.cc
 * @Description:
 */
#include "grpc_client.h"
#include "configmgr.h"

namespace core
{

    VerifyGrpcClient::VerifyGrpcClient()
    {
        auto &config_mgr = ConfigMgr::GetInstance();
        std::string rpcPoolNum = config_mgr["RpcConnPool"]["VerifyServer"];
        std::string host = config_mgr["VerifyServer"]["host"];
        std::string port = config_mgr["VerifyServer"]["port"];
        _pool.reset(new RpcConnPool<VerifyService>(std::stoi(rpcPoolNum.c_str()), host, port));
    }

    GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email)
    {
        ClientContext context;
        GetVerifyReq request;
        GetVerifyRsp response;
        request.set_email(email);

        // 通过 存根 调用 RPC 方法
        auto stub = _pool->getConnection();
        Status status = stub->GetVerifyCode(&context, request, &response);

        if (status.ok())
        {
            _pool->returnConnection(std::move(stub));
        }
        else
        {
            _pool->returnConnection(std::move(stub));
            response.set_error(ErrorCodes::RPCFailed);
        }

        return response;
    }

    StatusGrpcClient::StatusGrpcClient()
    {
        auto &config_mgr = ConfigMgr::GetInstance();
        std::string poolNum = config_mgr["RpcConnPool"]["StatusServer"];
        std::string host = config_mgr["StatusServer"]["host"];
        std::string port = config_mgr["StatusServer"]["port"];
        _pool.reset(new RpcConnPool<StatusService>(std::stoi(poolNum), host, port));
    }

    GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
    {
        ClientContext context;
        GetChatServerRsp response;
        GetChatServerReq request;
        request.set_uid(uid);
        auto stub = _pool->getConnection();
        Status status = stub->GetChatServer(&context, request, &response);
        Defer defer([&stub, this]{
            _pool->returnConnection(std::move(stub));
        });

        if(status.ok()){
            return response;
        }else{
            response.set_error(ErrorCodes::RPCFailed);
            return response;
        }
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
        Defer defer([&stub, this]{
            _pool->returnConnection(std::move(stub));
        });

        if(status.ok()){
            return response;
        }else{
            response.set_error(ErrorCodes::RPCFailed);
            return response;
        }
    }

}