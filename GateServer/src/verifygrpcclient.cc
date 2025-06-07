/*
 * @Author: star-cs
 * @Date: 2025-06-06 20:53:34
 * @LastEditTime: 2025-06-07 23:17:33
 * @FilePath: /CChat_server/GateServer/src/verifygrpcclient.cc
 * @Description:
 */
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
        auto stub = _pool->getConnection();
        Status status = stub->GetVerifyCode(&context, request, &response);

        if (status.ok()) {
            _pool->returnConnection(std::move(stub));
        }
        else {
            _pool->returnConnection(std::move(stub));
            response.set_error(ErrorCodes::RPCFailed);
        }

        return response;
    }

    VerifyGrpcClient::VerifyGrpcClient()
    {
        auto &config_mgr = ConfigMgr::GetInstance();
        std::string rpcPoolNum = config_mgr["RpcCoonPool"]["VerifyServer"];
        std::string host = config_mgr["VerifyServer"]["host"];
        std::string port = config_mgr["VerifyServer"]["port"];
        _pool.reset(new RpcConnPool(std::stoi(rpcPoolNum.c_str()), host, port));
    }
    
    // RPC pool

    RpcConnPool::RpcConnPool(size_t poolSize, std::string host, std::string port)
        : _b_stop(false), _poolSize(poolSize), _host(host), _port(port)
    {
        for(std::size_t i = 0; i < _poolSize ; ++i){
            std::shared_ptr<Channel> channel = grpc::CreateChannel(host + ":" + port, grpc::InsecureChannelCredentials()); // gRPC channel
            // 这里 NewStub 返回的是 unique_ptr，没有拷贝构造
            // push 直接 移动优化
            // _connections.push(VerifyService::NewStub(channel));
            // emplace 也行，更好
            _connections.emplace(VerifyService::NewStub(channel));
        }
    }


    RpcConnPool::~RpcConnPool()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        Close();
        while(!_connections.empty()){
            _connections.pop();
        }
    }

    std::unique_ptr<VerifyService::Stub> RpcConnPool::getConnection(){
        std::unique_lock<std::mutex> lock(_mutex);
        _cond.wait(lock, [this](){
            if(_b_stop) return true;
            return !_connections.empty();
        });

        auto context = std::move(_connections.front());
        _connections.pop();
        return context;
    }

    void RpcConnPool::returnConnection(std::unique_ptr<VerifyService::Stub> context){
        std::unique_lock<std::mutex> lock(_mutex);
        if (_b_stop) {
            return;
        }
        _connections.push(std::move(context));
        _cond.notify_one();
    }

    void RpcConnPool::Close(){
        _b_stop = true;
        _cond.notify_all();
    }
}