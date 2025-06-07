/*
 * @Author: star-cs
 * @Date: 2025-06-06 09:55:24
 * @LastEditTime: 2025-06-07 23:58:33
 * @FilePath: /CChat_server/GateServer/main.cc
 * @Description: GateServer 网关服务器 main
 */
#include "src/common.h"
#include "src/configmgr.h"
#include "src/cserver.h"
#include "src/asio_io_service_pool.h"

int main()
{
    try
    {
        core::ConfigMgr gCfgMgr = core::ConfigMgr::GetInstance();
        std::string gate_port_str = gCfgMgr["GateServer"]["port"];
        unsigned short port = atoi(gate_port_str.c_str());

        std::string threadNum = gCfgMgr["GateServer"]["threadNum"];
        std::size_t ioServicePoolnum = std::stoul(threadNum);
        // 初始化 AsioIOServicePool
        core::AsioIOServicePool::GetInstance()->Init(ioServicePoolnum);

        net::io_context io_context{1};
        net::signal_set signals(io_context, SIGINT, SIGTERM); // SIGINT通过Ctrl+C触发，​ SIGTERM通过kill命令触发
        signals.async_wait([&io_context](auto, auto)          // 回调函数形参：boost::system::error_code: 表示操作的错误状态，int: 表示捕获的信号编号。
                           { io_context.stop(); });

                std::make_shared<core::CServer>(io_context, port)->Start();
        std::cout << "Gate Server listen on port: " << port << std::endl;
        io_context.run();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception：" << e.what() << std::endl;
    }
}