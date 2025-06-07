/*
 * @Author: star-cs
 * @Date: 2025-06-07 21:00:08
 * @LastEditTime: 2025-06-07 23:56:29
 * @FilePath: /CChat_server/GateServer/src/asio_io_service_pool.cc
 * @Description:
 */
#include "asio_io_service_pool.h"

namespace core
{
    /**
     * 延迟初始化
     *
     * 新版本
     * boost::asio::executor_work_guard 没有默认构造了
     * works_.(size) 没用了
     */
    void AsioIOServicePool::Init(std::size_t size)
    {
        _nextIOService = 0;
        // 先清空，避免重复调用时积累旧数据
        _ioServices.clear();
        _works.clear();
        _threads.clear();

        // 创建 io_context 和对应的 work_guard
        for (std::size_t i = 0; i < size; ++i)
        {
            /**
             *  新版 work_guard 写法
             *
             * 源码：
             * template <typename Executor>
             * executor_work_guard<Executor> make_work_guard(const Executor& ex);
             *
             * io_context 隐式转换为 executor_type
             *
             */
            auto ioc = std::make_shared<boost::asio::io_context>();
            _works.emplace_back(boost::asio::make_work_guard(*ioc));
            _ioServices.emplace_back(ioc);
        }

        // 遍历多个ioService，创建多个线程，每个线程内部启动ioService
        for (std::size_t i = 0; i < _ioServices.size(); ++i)
        {
            _threads.emplace_back([this, i]()
                                  { _ioServices[i]->run(); });
        }
    }

    AsioIOServicePool::~AsioIOServicePool()
    {
        Stop();
        std::cout << "AsioIOServicePool destruct" << std::endl;
    }

    boost::asio::io_context &AsioIOServicePool::GetIOService()
    {
        auto &cur = _ioServices[_nextIOService++];
        if (_nextIOService == _ioServices.size())
        {
            _nextIOService = 0;
        }
        return *cur;
    }

    void AsioIOServicePool::Stop()
    {
        // 先释放所有 work_guard，让 run() 可以自然退出
        for (auto &work : _works)
        {
            work.reset();
        }
        // 再显式 stop 防止已有监听阻塞
        for (auto &ios : _ioServices)
        {
            ios->stop();
        }
        for (auto &t : _threads)
        {
            t.join();
        }
    }
}