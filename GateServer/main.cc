/*
 * @Author: star-cs
 * @Date: 2025-06-06 09:55:24
 * @LastEditTime: 2025-07-03 20:01:06
 * @FilePath: /CChat_server/GateServer/main.cc
 * @Description: GateServer 网关服务器 main
 */
#include "common.h"
#include "configmgr.h"
#include "env.h"
#include "src/cserver.h"
#include "asio_io_service_pool.h"
#include "redis_mgr.h"
#include "mysql_mgr.h"
#include "src/grpc_client.h"

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
    auto logger = coro::log::logger::get_logger();
    std::shared_ptr<core::CServer> server;
    try
    {
        std::string gate_port_str = cfg["GateServer"]["port"];
        unsigned short port = boost::lexical_cast<unsigned short>(gate_port_str);

        core::RedisMgr::GetInstance();

        core::MysqlMgr::GetInstance();

        core::AsioIOServicePool::GetInstance();

        core::VerifyGrpcClient::GetInstance();
        core::StatusGrpcClient::GetInstance();

        net::io_context io_context{1};
        net::signal_set signals(io_context, SIGINT, SIGTERM); // SIGINT通过Ctrl+C触发，​ SIGTERM通过kill命令触发
        signals.async_wait([&io_context](auto, auto)          // 回调函数形参：boost::system::error_code: 表示操作的错误状态，int: 表示捕获的信号编号。
                           { io_context.stop(); });

        server = std::make_shared<core::CServer>(io_context, port);
        LOG_INFO("GateServer listening on {}", port);
        server->Start();
        io_context.run();
    }
    catch (std::exception &e)
    {
        LOG_ERROR("Exception：{}", e.what());
        return 1;
    }
    return 0;
}