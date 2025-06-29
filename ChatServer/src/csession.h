/*
 * @Author: star-cs
 * @Date: 2025-06-15 21:16:19
 * @LastEditTime: 2025-06-29 16:42:25
 * @FilePath: /CChat_server/ChatServer/src/csession.h
 * @Description:
 */
#pragma once
#include "common.h"
#include "msg_node.h"
#include <cstddef>
#include <memory>
namespace core
{
class CServer;
class LogicSystem;

class CSession : public std::enable_shared_from_this<CSession>
{
public:
    CSession(boost::asio::io_context &io_context, CServer *server);
    ~CSession();
    tcp::socket &GetSocket() { return _socket; }

    std::string &GetUuid() { return _session_id; }

    int GetUserId() { return _user_id; }
    void SetUserId(int uid) { _user_id = uid; }

    std::string GetSessionId() { return _session_id; }

    void Start();
    void Send(char *msg, short max_length, short msgid);
    void Send(std::string msg, short msgid);
    void Close();
    void AsyncReadBody(int length);
    void AsyncReadHead(int total_len);
    void NotifyOffline(int uid);
    bool IsHeartbeatExpired(time_t now);
    void UpdateHeartbeat() { _last_heartbeat = std::time(nullptr); }
    void DealExceptionSession();


private:
    // 这个回调 Send中 通过 async_write 触发
    // 会回调嵌套，直到把 队列的消息 消费完
    void HandleWrite(const boost::system::error_code &error, std::shared_ptr<CSession> shared_self);

private:
    tcp::socket _socket;
    std::string _session_id;
    int _user_id; // 客户端 uid
    char _data[MAX_LENGTH];
    CServer *_server;
    bool _b_close;
    std::queue<std::shared_ptr<SendNode>> _send_que;
    std::mutex _send_lock;
    // 存放 收到的消息结构
    std::shared_ptr<RecvNode> _recv_msg_node;
    bool _b_head_parse;
    // 收到的头部结构
    std::shared_ptr<MsgNode> _recv_head_node;
    time_t _last_heartbeat;
};

class LogicNode
{
    friend class LogicSystem;

public:
    LogicNode(std::shared_ptr<CSession> session, std::shared_ptr<RecvNode> recvNode)
        : _session(session), _recvNode(recvNode)
    {
    }

private:
    std::shared_ptr<CSession> _session;
    std::shared_ptr<RecvNode> _recvNode;
};

} // namespace core
