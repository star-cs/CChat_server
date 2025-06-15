/*
 * @Author: star-cs
 * @Date: 2025-06-14 16:08:02
 * @LastEditTime: 2025-06-14 17:30:40
 * @FilePath: /CChat_server/StatusServer/src/status_service_impl.cc
 * @Description:
 */
#include "status_service_impl.h"
#include "configmgr.h"
#include "common.h"
#include <boost/algorithm/string.hpp>
#include <boost/uuid.hpp>

namespace core
{
    std::string generate_unique_string(){
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
        const ChatServer& server = getChatServer();
        response->set_error(ErrorCodes::Success);
        response->set_host(server.host);
        response->set_port(server.port);
        response->set_token(generate_unique_string());
        return Status::OK;
    }

    Status StatusServiceImpl::Login(ServerContext *context, const LoginReq *request, LoginRsp *response)
    {
        return Status::OK;
    }

    void StatusServiceImpl::insertToken(int uid, std::string token)
    {
        
    }

    ChatServer StatusServiceImpl::getChatServer()
    {
        std::unique_lock<std::mutex> lock(_server_mtx);
        auto minServer = _servers.begin()->second;
        for (auto &server : _servers)
        {
            if(server.second.con_count < minServer.con_count){
                minServer = server.second;
            }
        }
        return minServer;
    }
}