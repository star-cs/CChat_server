#include "cserver.h"
#include "httpconnection.h"
#include "asio_io_service_pool.h"
namespace core
{
CServer::CServer(boost::asio::io_context &ioc, unsigned short &port)
    : _ioc(ioc), _acceptor(ioc, tcp::endpoint(tcp::v4(), port))
{
}

void CServer::Start()
{
    auto self = shared_from_this();
    // 线程池
    auto &ioc_ptr = AsioIOServicePool::GetInstance()->GetIOService();
    // 防止回调 HttpConnection 析构
    std::shared_ptr<HttpConnection> conn = std::make_shared<HttpConnection>(ioc_ptr);

    _acceptor.async_accept(conn->getSocket(), [self, conn](beast::error_code ec) {
        try {
            //出错则放弃这个连接，继续监听新链接
            if (ec) {
                self->Start();
                return;
            }
            conn->Start();
            //继续监听
            self->Start();
        } catch (std::exception &exp) {
            std::cout << "exception is " << exp.what() << std::endl;
            self->Start();
        }
    });
}
} // namespace core