/*
 * @Author: star-cs
 * @Date: 2025-06-06 09:55:24
 * @LastEditTime: 2025-06-12 19:55:08
 * @FilePath: /CChat_server/GateServer/src/httpconnection.h
 * @Description: 
 */
#pragma once
#include "common.h"

namespace core
{
    class HttpConnection : public std::enable_shared_from_this<HttpConnection>
    {        
        friend class LogicSystem;
    public:
        
        HttpConnection(boost::asio::io_context& ioc);
        void Start();
        tcp::socket& getSocket(){
            return _socket;
        } 

    private:
        void CheckDeadline();
        void WriteResponse();
        void HandleReq();
        void PreParseGetParam();

        tcp::socket _socket;    
        // The timer for putting a deadline on connection processing.
        net::steady_timer _deadline;
        // The buffer for performing reads.
        beast::flat_buffer _buffer{8192};
        // The request message.
        http::request<http::dynamic_body> _request;
        // The response message.
        http::response<http::dynamic_body> _response;
        // url 请求体
        std::string _get_url;
        // 请求url内附带的 key-value信息，因为是无序的所以使用 unordered_map
        std::unordered_map<std::string, std::string> _get_params;
    };

} // namespace star
