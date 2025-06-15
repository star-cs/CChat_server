#include "httpconnection.h"
#include "logicsystem.h"

namespace core
{
    HttpConnection::HttpConnection(boost::asio::io_context &ioc) : _socket(ioc), _deadline(ioc, std::chrono::seconds(60))
    {
    }

    void HttpConnection::Start()
    {
        // 为什么 beast::error_code 当作 bool 判断？
        // error_code 重载了 == 运算符
        auto self = shared_from_this();
        http::async_read(_socket, _buffer, _request, [self](beast::error_code ec, std::size_t bytes_transferrred)
                         {
            try{
                if (ec) {
                    std::cout << "http read err is " << ec.message() << std::endl;
                    return;
                }
                // 处理
                boost::ignore_unused(bytes_transferrred);
                self->HandleReq();
                self->CheckDeadline();
            }catch(std::exception& exp){
                std::cout << "exception is " << exp.what() << std::endl;
            } });
    }

    void HttpConnection::CheckDeadline()
    {
        auto self = shared_from_this();
        _deadline.async_wait([self](beast::error_code ec)
                             {
            if(!ec){
                self->_socket.close(ec);
            } });
    }

    void HttpConnection::WriteResponse()
    {
        auto self = shared_from_this();
        _response.content_length(_response.body().size());
        http::async_write(_socket, _response, [self](beast::error_code ec, std::size_t bytes_transferrred)
                          {
            try{
                // http 单次连接，发送完响应后，_socket 关闭 写段
                self->_socket.shutdown(tcp::socket::shutdown_send, ec);
                self->_deadline.cancel();
            }catch(std::exception& exp){
                std::cout << "exception is " << exp.what() << std::endl;
            } });
    }

    void HttpConnection::HandleReq()
    {
        // 设置版本
        _response.version(_request.version());
        _response.keep_alive(false);

        if (_request.method() == http::verb::get)
        {
            PreParseGetParam();
            bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
            if (!success)
            {
                _response.result(http::status::not_found);
                _response.set(http::field::content_type, "text/plain");
                beast::ostream(_response.body()) << "url not found\r\n";
                WriteResponse(); // 发送
                return;
            }

            _response.result(http::status::ok);
            _response.set(http::field::server, "GateServer");
            WriteResponse();
            return;
        }
        else if (_request.method() == http::verb::post)
        {
            bool success = LogicSystem::GetInstance()->HandlePost(std::string(_request.target().data()), shared_from_this());
            if (!success)
            {
                _response.result(http::status::not_found);
                _response.set(http::field::content_type, "text/plain");
                beast::ostream(_response.body()) << "url not found\r\n";
                WriteResponse(); // 发送
                return;
            }

            _response.result(http::status::ok);
            _response.set(http::field::server, "GateServer");
            WriteResponse();
            return;
        }
    }

    void HttpConnection::PreParseGetParam()
    {
        std::string uri(_request.target());
        std:size_t query_pos = uri.find("?");
        if (query_pos == std::string::npos)
        { // 没有 k-v 数据
            _get_url = uri;
            return;
        }

        _get_url = uri.substr(0, query_pos);
        std::string query_string = uri.substr(query_pos + 1);
        std::string key;
        std::string value;
        std::size_t pos = 0;
        do
        {
            pos = query_string.find("&");
            auto pair_str = query_string.substr(0, pos);

            std::size_t eq_pos = pair_str.find("=");
            if (eq_pos != std::string::npos)
            {
                key = UrlDecode(pair_str.substr(0, eq_pos));
                value = UrlDecode(pair_str.substr(eq_pos + 1));

                _get_params[key] = value;
            }
            query_string.erase(0, pos + 1);

        } while (pos != std::string::npos);
    }

}