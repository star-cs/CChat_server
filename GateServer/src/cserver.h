/*
 * @Author: star-cs
 * @Date: 2025-06-06 09:55:25
 * @LastEditTime: 2025-06-16 20:00:27
 * @FilePath: /CChat_server/GateServer/src/cserver.h
 * @Description: 
 */
#pragma once

#include "common.h"

namespace core
{
    class CServer : public std::enable_shared_from_this<CServer>
    {
    public:
        CServer(net::io_context &ioc, unsigned short &port);
        void Start();
    private:
        tcp::acceptor _acceptor;
        net::io_context &_ioc;
    };
}