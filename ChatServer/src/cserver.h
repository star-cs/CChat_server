/*
 * @Author: star-cs
 * @Date: 2025-06-28 11:54:15
 * @LastEditTime: 2025-06-29 19:21:12
 * @FilePath: /CChat_server/ChatServer/src/cserver.h
 * @Description: 
 */
#pragma once

#include "common.h"
#include "csession.h"
#include <boost/asio/steady_timer.hpp>

namespace core
{
class CServer : public std::enable_shared_from_this<CServer>
{
public:
    CServer(boost::asio::io_context &ioc, unsigned short &port);
    ~CServer();
    void ClearSession(std::string session);
    bool CheckValid(std::string sessionId);
    void on_timer(const boost::system::error_code &error);
    void StartTimer();
    void StopTimer();

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
    boost::asio::steady_timer _timer;
};
} // namespace core