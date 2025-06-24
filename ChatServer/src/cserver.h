#pragma once

#include "common.h"
#include "csession.h"

namespace core
{
class CServer : public std::enable_shared_from_this<CServer>
{
public:
    CServer(boost::asio::io_context &ioc, unsigned short &port);
    ~CServer();
    void ClearSession(std::string session);

private:
    void HandleAccept(std::shared_ptr<CSession> new_session,
                      const boost::system::error_code &error);
    void StartAccept();

private:
    net::io_context &_ioc;
    unsigned short _port;
    tcp::acceptor _acceptor;
    std::map<std::string, std::shared_ptr<CSession>> _sessions;
    std::mutex _mutex;
};
} // namespace core