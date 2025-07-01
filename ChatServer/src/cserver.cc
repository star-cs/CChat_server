/*
 * @Author: star-cs
 * @Date: 2025-06-15 20:44:34
 * @LastEditTime: 2025-06-29 20:02:19
 * @FilePath: /CChat_server/ChatServer/src/cserver.cc
 * @Description:
 */
#include "cserver.h"
#include "asio_io_service_pool.h"
#include "common.h"
#include "csession.h"
#include "logic_system.h"
#include "user_mgr.h"
#include "configmgr.h"
#include "redis_mgr.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace core
{
CServer::CServer(boost::asio::io_context &ioc, unsigned short &port)
    : _ioc(ioc), _port(port), _acceptor(ioc, tcp::endpoint(tcp::v4(), port)),
      _timer(ioc, std::chrono::seconds(60))
{
    StartAccept();
}

CServer::~CServer()
{
    std::cout << "Server destruct listen on port : " << _port << std::endl;
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

void CServer::on_timer(const boost::system::error_code &error)
{
    // 心跳定时器
    std::cout << "on_timer" << std::endl;
    if (error) {
        std::cout << "timer error: " << error.message() << std::endl;
        return;
    }
    std::vector<std::shared_ptr<CSession>> _expired_session;
    int session_count = 0;

    std::map<std::string, std::shared_ptr<CSession>> sessions_copy;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        sessions_copy = _sessions;
    }

    time_t now = std::time(nullptr);
    for (auto iter = sessions_copy.begin(); iter != sessions_copy.end(); iter++) {
        auto b_expire = iter->second->IsHeartbeatExpired(now);
        if (b_expire) {
            iter->second->Close();
            _expired_session.push_back(iter->second);
            continue;
        }
        session_count++;
    }

    // 更新 LOGIN_COUNT 数量
    RedisMgr::GetInstance()->HSet(LOGIN_COUNT, ConfigMgr::GetInstance().GetSelfName(),
                                  std::to_string(session_count));

    // 处理过期session
    for (auto &session : _expired_session) {
        session->DealExceptionSession();
    }

    _timer.expires_after(std::chrono::seconds(60));
    _timer.async_wait([this](boost::system::error_code error) { on_timer(error); });
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

void CServer::StartTimer()
{
    auto self(shared_from_this());
    _timer.async_wait([self](boost::system::error_code ec) { self->on_timer(ec); });
}
void CServer::StopTimer()
{
    _timer.cancel();
}

} // namespace core