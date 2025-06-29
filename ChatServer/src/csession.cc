/*
 * @Author: star-cs
 * @Date: 2025-06-15 21:16:33
 * @LastEditTime: 2025-06-29 11:07:16
 * @FilePath: /CChat_server/ChatServer/src/csession.cc
 * @Description:
 */
#include "csession.h"
#include "common.h"
#include "cserver.h"
#include "logic_system.h"
#include "redis_mgr.h"
#include "configmgr.h"
#include "user_mgr.h"
#include "json/value.h"
#include <boost/uuid.hpp>
#include <memory>
#include <mutex>

namespace core
{
CSession::CSession(boost::asio::io_context &io_context, CServer *server)
    : _socket(io_context), _server(server), _b_close(false), _b_head_parse(false)
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    _session_id = boost::uuids::to_string(uuid);
    _recv_head_node = std::make_shared<MsgNode>(HEAD_TOTAL_LEN);
}

CSession::~CSession()
{
    std::cout << "~CSession destruct" << std::endl;
}

void CSession::Start()
{
    AsyncReadHead(HEAD_TOTAL_LEN);
}

void CSession::Send(char *msg, short max_length, short msgid)
{
    std::unique_lock<std::mutex> lock(_send_lock);
    int send_que_size = _send_que.size();
    if (send_que_size > MAX_SENDQUE) {
        LOG_ERROR("session: {} send que fulled, size is {}", _session_id, MAX_SENDQUE);
        return;
    }

    // 通过队列保证，消息发送的顺序性
    _send_que.push(std::make_shared<SendNode>(msg, max_length, msgid));

    // 之前还有节点无需再次 async_write
    // 因为，HandleWrite 会回调嵌套，直到把 队列的消息 消费完
    if (send_que_size > 0) {
        return;
    }

    auto &msgnode = _send_que.front();
    boost::asio::async_write(
        _socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
        std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_from_this()));
}

void CSession::Send(std::string msg, short msgid)
{
    Send(msg.data(), msg.size(), msgid);
}

void CSession::Close()
{
    _socket.close();
    _b_close = true;
}

void CSession::AsyncReadHead(int length)
{
    auto self = shared_from_this();
    ::memset(_data, 0, MAX_LENGTH);
    boost::asio::async_read(
        _socket, boost::asio::buffer(_data, length), boost::asio::transfer_exactly(length),
        [this, self, length](const boost::system::error_code &ec, std::size_t bytes_read) {
            try {
                if (ec) {
                    LOG_ERROR("AsyncReadHead error is {}", ec.message());
                    Close();
                    DealExceptionSession();
                    return;
                }

                if (bytes_read < length) {
                    LOG_ERROR("read length not match, read [{}] , total [{}] ", bytes_read,
                              HEAD_TOTAL_LEN);
                    Close();
                    _server->ClearSession(_session_id);
                    return;
                }

                //判断连接无效
                if (!_server->CheckValid(_session_id)) {
                    Close();
                    return;
                }

                _recv_head_node->Clear();
                // 解析 _data 中的头部信息
                memcpy(_recv_head_node->_data, _data, bytes_read);

                // 网络字节序 --> 主机字节序
                short msg_id = 0;
                memcpy(&msg_id, _recv_head_node->_data, HEAD_ID_LEN);
                msg_id = boost::asio::detail::socket_ops::network_to_host_short(msg_id);

                short msg_len = 0;
                memcpy(&msg_len, _recv_head_node->_data + HEAD_ID_LEN, HEAD_TOTAL_LEN);
                msg_len = boost::asio::detail::socket_ops::network_to_host_short(msg_len);

                if (msg_len > MAX_LENGTH) {
                    LOG_ERROR("invalid data length is {}", msg_len);
                    _server->ClearSession(_session_id);
                    return;
                }

                _recv_msg_node.reset(new RecvNode(msg_len, msg_id));
                AsyncReadBody(msg_len);
            } catch (const std::exception &e) {
                LOG_ERROR("Exception code is {}", e.what());
            }
        });
}

void CSession::AsyncReadBody(int total_len)
{
    auto self = shared_from_this();
    ::memset(_data, 0, MAX_LENGTH);
    boost::asio::async_read(
        _socket, boost::asio::buffer(_data, total_len), boost::asio::transfer_exactly(total_len),
        [this, self, total_len](const boost::system::error_code &ec, std::size_t bytes_read) {
            try {
                if (ec) {
                    LOG_ERROR("AsyncReadBody error is {}", ec.message());
                    Close();
                    DealExceptionSession();
                    return;
                }

                if (bytes_read < total_len) {
                    LOG_ERROR("read length not match, read [{}] , total [{}] ", bytes_read,
                              HEAD_TOTAL_LEN);
                    Close();
                    _server->ClearSession(_session_id);
                    return;
                }

                //判断连接无效
                if (!_server->CheckValid(_session_id)) {
                    Close();
                    return;
                }

                ::memcpy(_recv_msg_node->_data, _data,
                         bytes_read); // 客户端端发送过来的消息是保持了原始字节序
                _recv_msg_node->_cur_len += bytes_read;
                _recv_msg_node->_data[_recv_msg_node->_total_len] =
                    '\0'; // 方便识别到 \0 字符串进行截断
                LOG_INFO("receive data is {}", _recv_msg_node->_data);

                // 将消息投递到逻辑队列中
                LogicSystem::GetInstance()->PostMsgToQue(
                    std::make_shared<LogicNode>(shared_from_this(), _recv_msg_node));

                // 继续监听头部接受事件
                AsyncReadHead(HEAD_TOTAL_LEN);
            } catch (const std::exception &e) {
                LOG_ERROR("Exception code is {}", e.what());
            }
        });
}

void CSession::HandleWrite(const boost::system::error_code &error,
                           std::shared_ptr<CSession> shared_self)
{
    try {
        if (!error) {
            std::unique_lock<std::mutex> lock(_send_lock);
            // 发送成功
            _send_que.pop();
            if (!_send_que.empty()) {
                // 异步回调的不好的地方，这种嵌套回调的 可读性太差。
                auto &msgNode = _send_que.front();
                boost::asio::async_write(
                    shared_self->GetSocket(),
                    boost::asio::buffer(msgNode->_data, msgNode->_total_len),
                    std::bind(&CSession::HandleWrite, this, std::placeholders::_1, shared_self));
            }
        } else {
            LOG_ERROR("handle write failed, error is {}", error.message());
            Close();
            DealExceptionSession();
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Exception code is {}", e.what());
    }
}

void CSession::DealExceptionSession()
{
    auto self = shared_from_this();
    //加锁清除session
    auto uid_str = std::to_string(_user_id);
    auto lock_key = LOCK_PREFIX + uid_str;
    auto identifier =
        RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);
    Defer defer([identifier, lock_key, self, this]() {
        _server->ClearSession(_session_id);
        RedisMgr::GetInstance()->releaseLock(lock_key, identifier);
    });

    if (identifier.empty()) {
        return;
    }

    std::string redis_session_id = "";
    auto bsuccess = RedisMgr::GetInstance()->Get(USER_SESSION_PREFIX + uid_str, redis_session_id);
    if (!bsuccess) {
        return;
    }

    if (redis_session_id != _session_id) {
        //说明有客户在其他服务器异地登录了

        RedisMgr::GetInstance()->DecreaseCount(ConfigMgr::GetInstance().GetSelfName());

        return;
    }

    RedisMgr::GetInstance()->Del(USER_SESSION_PREFIX + uid_str);
    //清除用户登录信息
    RedisMgr::GetInstance()->Del(USERIPPREFIX + uid_str);
}

void CSession::NotifyOffline(int uid)
{
    Json::Value rsp;

    rsp["error"] = ErrorCodes::Success;
    rsp["uid"] = uid;

    Send(rsp.toStyledString(), ID_NOTIFY_OFF_LINE_REQ);
}

} // namespace core