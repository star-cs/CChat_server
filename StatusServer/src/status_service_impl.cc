/*
 * @Author: star-cs
 * @Date: 2025-06-14 16:08:02
 * @LastEditTime: 2025-06-22 14:24:48
 * @FilePath: /CChat_server/StatusServer/src/status_service_impl.cc
 * @Description:
 */
#include "status_service_impl.h"
#include "configmgr.h"
#include "common.h"
#include "redis_mgr.h"
#include <boost/algorithm/string.hpp>
#include <boost/uuid.hpp>

namespace core
{
    std::string generate_unique_string()
    {
        // 创建UUID对象
        boost::uuids::uuid uuid = boost::uuids::random_generator()();

        // 将UUID转换为字符串
        std::string unique_str(to_string(uuid));

        return unique_str;
    }

    StatusServiceImpl::StatusServiceImpl()
    {
        auto &cfg = ConfigMgr::GetInstance();
        std::string server_list = cfg["ChatServer"]["name"];
        std::vector<std::string> words;
        boost::split(words, server_list, boost::is_any_of(",")); // 按逗号分割
        for (auto &work : words)
        {
            if (cfg[work]["name"].empty())
            {
                continue;
            }
            ChatServer server;
            server.name = cfg[work]["name"];
            server.host = cfg[work]["host"];
            server.port = cfg[work]["port"];
            _servers[server.name] = server;
        }
    }
    Status StatusServiceImpl::GetChatServer(ServerContext *context, const GetChatServerReq *request, GetChatServerRsp *response)
    {
        LOG_INFO("StatusServer has received RPC Service : GetChatServer From GateServer");
        const ChatServer &server = getChatServer();
        response->set_error(ErrorCodes::Success);
        response->set_host(server.host);
        response->set_port(server.port);
        response->set_token(generate_unique_string());
        insertToken(request->uid(), response->token()); // 保存 uid -- token 对应关系
        return Status::OK;
    }

    Status StatusServiceImpl::Login(ServerContext *context, const LoginReq *request, LoginRsp *response)
    {
        LOG_INFO("StatusServer has received RPC Service : Login From ChatServer");
        auto uid = request->uid();
        auto token = request->token();
        // 查找 uid -- token 匹配关系 是否存在

        std::string token_key = USERTOKENPREFIX + std::to_string(uid);
        std::string token_value = "";

        // RedisMgr获取连接，会保证线程安全
        bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
        if (!success)
        {
            response->set_error(ErrorCodes::UidInvalid);
            return Status::OK;
        }
        if (token != token_value)
        {
            response->set_error(ErrorCodes::TokenInvalid);
            return Status::OK;
        }
        response->set_error(ErrorCodes::Success);
        response->set_uid(uid);
        response->set_token(token);
        return Status::OK;
    }

    void StatusServiceImpl::insertToken(int uid, std::string token)
    {
        // 写入到 Redis
        std::string uid_str = std::to_string(uid);
        std::string token_key = USERTOKENPREFIX + uid_str;
        RedisMgr::GetInstance()->Set(token_key, token);
    }

    ChatServer StatusServiceImpl::getChatServer()
    {
        ChatServer minServer;
        int minCount = INT_MAX;
        for (auto &server : _servers) {
            // 统一更新所有服务器的连接数
            std::string count_str = RedisMgr::GetInstance()->HGet(LOGIN_COUNT, server.second.name);
            int currentCount = count_str.empty() ? INT_MAX : std::stoi(count_str);
            server.second.con_count = currentCount;

            // 直接比较当前连接数
            if (currentCount < minCount) {
                minCount = currentCount;
                minServer = server.second;
            }
        }
        return minServer;
    }
}