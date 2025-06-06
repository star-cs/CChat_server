#pragma once
#include "common.h"

namespace core
{
    class HttpConnection : public std::enable_shared_from_this<HttpConnection>
    {
    public:
        friend class LogicSystem;
        
        HttpConnection(tcp::socket socket);
        void Start();

    private:
        void CheckDeadline();
        void WriteResponse();
        void HandleReq();
        void PreParseGetParam();

        tcp::socket _socket;    
        // The buffer for performing reads.
        beast::flat_buffer _buffer{8192};
        // The request message.
        http::request<http::dynamic_body> _request;
        // The response message.
        http::response<http::dynamic_body> _response;
        // The timer for putting a deadline on connection processing.
        net::steady_timer deadline_{
            _socket.get_executor(), std::chrono::seconds(60)};

        // url 请求体
        std::string _get_url;
        // 请求url内附带的 key-value信息，因为是无序的所以使用 unordered_map
        std::unordered_map<std::string, std::string> _get_params;
    };

} // namespace star
