/*
 * @Author: star-cs
 * @Date: 2025-06-14 15:33:20
 * @LastEditTime: 2025-06-16 18:29:47
 * @FilePath: /CChat_server/StatusServer/main.cc
 * @Description:
 */

#include "src/common.h"
#include "src/redis_mgr.h"
#include "src/configmgr.h"
#include "src/status_service_impl.h"

/**
 * 1. 基础服务基类
 * 重写.proto中定义的虚函数，处理业务逻辑
 * core::StatusServiceImpl
 *
 * 2. 创建服务器流程
 * core::StatusServiceImpl service;
 * grpc::ServerBuilder builder;
 * builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
 * builder.RegisterService(&service);
 * std::unique_ptr<Server> server(builder.BuildAndStart());
 * server->Wait();
 *
 */

void RunServer()
{
    auto &cfg = core::ConfigMgr::GetInstance();

    core::RedisMgr::GetInstance();

    std::string server_addr(cfg["StatusServer"]["host"] + ":" + cfg["StatusServer"]["port"]);

    core::StatusServiceImpl service;

    grpc::ServerBuilder builder;
    // 监听端口 添加服务
    builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    // 构建并启动gRPC服务器
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    LOG_INFO("Server listening on {}", server_addr);

    // 创建Boost.Asio的io_context
    boost::asio::io_context ioc;
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);

    signals.async_wait([&server, &ioc](const boost::system::error_code &error, int signal_number)
                       {
        if(!error){
            LOG_INFO("Shutting down server...");
            server->Shutdown();
            ioc.stop();
        }; });

    std::thread([&ioc]()
                { ioc.run(); })
        .detach();

    // 等待服务器关闭
    server->Wait();
}

int main()
{
    auto logger = coro::log::logger::get_logger();
    try
    {
        RunServer();
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Error: ", e.what());
        return EXIT_FAILURE;
    }
    return 0;
}