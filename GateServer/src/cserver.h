#pragma once

#include "common.h"

namespace core
{
    class CServer : public std::enable_shared_from_this<CServer>
    {
    public:
        CServer(net::io_context &ioc, unsigned short &port);
        ~CServer();

        void Start();

    private:
        tcp::acceptor _acceptor;
        net::io_context &_ioc;
        tcp::socket _socket;
    };
}