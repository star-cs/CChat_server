/*
 * @Author: star-cs
 * @Date: 2025-06-15 20:37:19
 * @LastEditTime: 2025-06-30 19:12:04
 * @FilePath: /CChat_server/ChatServer/main.cc
 * @Description:
 */
#include "env.h"
#include "common.h"
#include "asio_io_service_pool.h"
#include "configmgr.h"
#include "src/cserver.h"
#include "mysql_mgr.h"
#include "src/grpc_client.h"
#include "redis_mgr.h"
#include "src/grpc_chat_service_impl.h"
#include "src/logic_system.h"
#include <boost/lexical_cast.hpp>
#include <memory>

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
    auto &cfg = core::ConfigMgr::GetInstance(config_name);
    auto server_name = cfg.GetSelfName();

    try {

        core::MysqlMgr::GetInstance();
        core::RedisMgr::GetInstance();

        auto pool = core::AsioIOServicePool::GetInstance();

        // 将登录个数设置为0
        core::RedisMgr::GetInstance()->InitCount(server_name);
        core::Defer derfer([server_name]() {
            core::RedisMgr::GetInstance()->HDel(LOGIN_COUNT, server_name);
            core::RedisMgr::GetInstance()->Close();
        });

        boost::asio::io_context io_context;

        // C++中非const左值引用无法绑定到右值​（临时对象）
        // auto port_str = cfg["SelfServer"]["port"];
        // core::CServer s(io_context, boost::lexical_cast<unsigned short>(port_str.c_str()));
        unsigned short port = boost::lexical_cast<unsigned short>(cfg.GetSelfPort());
        std::shared_ptr<core::CServer> cserver = std::make_shared<core::CServer>(io_context, port);
        cserver->StartTimer();
        LOG_INFO("TCP Server {} listening on {}:{}", server_name, cfg.GetSelfHost(), port);

        // 定义 GrpcServer
        std::string server_address(cfg["SelfServer"]["host"] + ":" + cfg["SelfServer"]["RPCPort"]);
        core::ChatServiceImpl service;

        grpc::ServerBuilder builder;

        // 监听端口和添加服务
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        service.RegisterServer(cserver);

        // 构建并启动 gRPC 服务器
        std::unique_ptr<grpc::Server> gpcserver(builder.BuildAndStart());

        LOG_INFO("gRPC Server {} listening on {}", server_name, server_address);

        std::thread grpc_server_thread([&gpcserver]() { gpcserver->Wait(); });

        core::StatusGrpcClient::GetInstance();
        core::ChatGrpcClient::GetInstance(); // 惰性加载 bug解决，先启动 server ~

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, pool, &gpcserver](auto, auto) {
            io_context.stop();
			pool->Stop();
			gpcserver->Shutdown();
        });

        // 注册系统 保存 CServer 的信息，方便在里面 对 _sessions 进行操作
        core::LogicSystem::GetInstance()->setCServer(cserver);

        io_context.run();
        grpc_server_thread.join();

        cserver->StopTimer();
    } catch (const std::exception &e) {
        LOG_ERROR("Exception {}", e.what());
    }
}