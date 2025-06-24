/*
 * @Author: star-cs
 * @Date: 2025-06-15 20:37:19
 * @LastEditTime: 2025-06-24 10:22:57
 * @FilePath: /CChat_server/ChatServer/main.cc
 * @Description:
 */
#include "src/env.h"
#include "src/common.h"
#include "src/asio_io_service_pool.h"
#include "src/configmgr.h"
#include "src/cserver.h"
#include "src/mysql_mgr.h"
#include "src/grpc_client.h"
#include "src/redis_mgr.h"
#include "src/grpc_chat_service_impl.h"
#include <boost/lexical_cast.hpp>

int main(int argc, char **argv)
{
    auto env = core::Env::GetInstance();
    env->parse(argc, argv);
    // 检查选项
    if (env->hasOption("h") || env->hasOption("help")) {
        std::cout << "Usage: " << argv[0] << " [options] [files...]\n"
                  << "Options:\n"
                  << "  -h, --help      Show help\n"
                  << "  -f <file>       config.ini file\n";
        return 0;
    }

    // 获取选项值
    std::string config_name = env->getOption("f", "config.ini");

    try {
        auto &cfg = core::ConfigMgr::GetInstance(config_name);

        core::MysqlMgr::GetInstance();
        core::RedisMgr::GetInstance();

        auto pool = core::AsioIOServicePool::GetInstance();

        core::StatusGrpcClient::GetInstance();

        auto server_name = cfg["SelfServer"]["name"];

        // 将登录个数设置为0
        core::RedisMgr::GetInstance()->HSet(LOGIN_COUNT, server_name, "0");

        // 定义 GrpcServer
        std::string server_address(cfg["SelfServer"]["host"] + ":" + cfg["SelfServer"]["RPCPort"]);
        core::ChatServiceImpl service;
        grpc::ServerBuilder builder;

        // 监听端口和添加服务
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        // 构建并启动 gRPC 服务器
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

        LOG_INFO("gRPC Server {} listening on {}", server_name, server_address);

        std::thread grpc_server_thread([&server]() { server->Wait(); });

        core::ChatGrpcClient::GetInstance();     // 惰性加载 bug解决，先启动 server ~ 

        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, pool, &server](auto, auto) {
            io_context.stop();
            pool->Stop();
            server->Shutdown();
        });

        // C++中非const左值引用无法绑定到右值​（临时对象）
        // auto port_str = cfg["SelfServer"]["port"];
        // core::CServer s(io_context, boost::lexical_cast<unsigned short>(port_str.c_str()));
        unsigned short port = boost::lexical_cast<unsigned short>(cfg["SelfServer"]["port"]);
        core::CServer s(io_context, port);
        LOG_INFO("TCP Server {} listening on {}:{}", server_name, cfg["SelfServer"]["host"], port);
        io_context.run();

        core::RedisMgr::GetInstance()->HDel(LOGIN_COUNT, server_name);
        core::RedisMgr::GetInstance()->Close();

        grpc_server_thread.join();
    } catch (const std::exception &e) {
        LOG_ERROR("Exception {}", e.what());
    }
}