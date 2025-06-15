/*
 * @Author: star-cs
 * @Date: 2025-06-06 09:55:24
 * @LastEditTime: 2025-06-12 19:59:45
 * @FilePath: /CChat_server/GateServer/src/cserver.cc
 * @Description:
 */

#include "cserver.h"
#include "httpconnection.h"
#include "asio_io_service_pool.h"
namespace core
{
    CServer::CServer(boost::asio::io_context &ioc, unsigned short &port)
        : _ioc(ioc), _acceptor(ioc)
    {
        init(port);
    }

    void CServer::init(unsigned short& port)
    {
        try
        {
            // 创建端点
            tcp::endpoint endpoint(tcp::v4(), port);

            // 1. 打开acceptor
            _acceptor.open(endpoint.protocol());

            _acceptor.set_option(tcp::acceptor::reuse_address(true));

            // 3. 绑定端口
            _acceptor.bind(endpoint);

            // 4. 开始监听
            _acceptor.listen();

            // 打印绑定信息
            LOG_INFO("Successfully bound to {}:{}", endpoint.address().to_string(), endpoint.port());
        }
        catch (const boost::system::system_error &e)
        {
            LOG_TRACE("Binding failed: {}", e.what());

            // 获取详细错误信息
            const auto &code = e.code();
            LOG_ERROR("Error category: {}, value: {}, message: {}",
                      code.category().name(), code.value(), code.message());

            throw; // 重新抛出异常
        }
    }

    
    void CServer::Start()
    {
        auto self = shared_from_this();
        // 线程池
        auto &ioc_ptr = AsioIOServicePool::GetInstance()->GetIOService();
        // 防止回调 HttpConnection 析构
        std::shared_ptr<HttpConnection> conn = std::make_shared<HttpConnection>(ioc_ptr);

        _acceptor.async_accept(conn->getSocket(), [self, conn](beast::error_code ec)
                               {
            try{
                //出错则放弃这个连接，继续监听新链接
                if(ec){
                    self->Start();
                    return;
                }
                conn->Start();
                //继续监听
                self->Start();
            }catch(std::exception& exp){
                std::cout << "exception is " << exp.what() << std::endl;
                self->Start();
            } });
    }
}