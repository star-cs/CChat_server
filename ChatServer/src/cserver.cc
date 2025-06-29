/*
 * @Author: star-cs
 * @Date: 2025-06-15 20:44:34
 * @LastEditTime: 2025-06-29 10:25:54
 * @FilePath: /CChat_server/ChatServer/src/cserver.cc
 * @Description:
 */
#include "cserver.h"
#include "asio_io_service_pool.h"
#include "logic_system.h"
#include "user_mgr.h"
#include "configmgr.h"

namespace core
{
CServer::CServer(boost::asio::io_context &ioc, unsigned short &port)
    : _ioc(ioc), _port(port), _acceptor(ioc, tcp::endpoint(tcp::v4(), port))
{
    // 注册系统 保存 CServer 的信息，方便在里面 对 _sessions 进行操作
    core::LogicSystem::GetInstance()->setCServer(this);
    StartAccept();
}

CServer::~CServer()
{
    std::cout << "Server destruct" << std::endl;
}

void CServer::ClearSession(std::string session_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _sessions.erase(session_id);
}

bool CServer::CheckValid(std::string sessionId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sessionId);
    if (it != _sessions.end()) {
        return true;
    }
    return false;
}

void CServer::HandleAccept(std::shared_ptr<CSession> new_session,
                           const boost::system::error_code &error)
{
    if (!error) {
        new_session->Start(); // session开始
        std::lock_guard<std::mutex> lock(_mutex);
        // 连接到 客户端，保存起来
        _sessions.insert(std::make_pair(new_session->GetUuid(), new_session));
    } else {
        LOG_ERROR("session accept failed, error is {}", error.value());
    }

    StartAccept();
}

void CServer::StartAccept()
{
    auto &ioc = AsioIOServicePool::GetInstance()->GetIOService();

    std::shared_ptr<CSession> new_session = std::make_shared<CSession>(ioc, this);

    _acceptor.async_accept(new_session->GetSocket(), std::bind(&CServer::HandleAccept, this,
                                                               new_session, std::placeholders::_1));
}

} // namespace core