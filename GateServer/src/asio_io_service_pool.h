/*
 * @Author: star-cs
 * @Date: 2025-06-07 21:00:03
 * @LastEditTime: 2025-06-07 21:47:34
 * @FilePath: /CChat_server/GateServer/src/asio_io_service_pool.h
 * @Description:  asio 线程池，每个线程独占一个 iocontext
 */

#pragma once
#include "common.h"
#include "singleton.h"

namespace core
{
    class AsioIOServicePool : public Singleton<AsioIOServicePool>
    {
        friend Singleton<AsioIOServicePool>;

    public:
        using IOService = boost::asio::io_context;
        // 每个 io_context 都有一个 executor_type，用来表示调度任务的接口
        // executor_work_guard 是 Asio 防止 io_context::run() 提前退出的守护器
        // executor_work_guard 的作用是：持有一个 executor，保持 io_context 活跃， 调用 reset() 或 析构时，释放。
        // 
        // 新boost版本，工作守卫
        using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;   
        ~AsioIOServicePool();
        AsioIOServicePool(const AsioIOServicePool &) = delete;
        AsioIOServicePool &operator=(const AsioIOServicePool &) = delete;

        // 使用 round-robin 的方式返回 一个 io_service
        boost::asio::io_context &GetIOService();
        void Stop();
        void Init(std::size_t size = 2);

    private:
        AsioIOServicePool() = default;
        std::vector<std::shared_ptr<IOService>> _ioServices;
        std::vector<Work> _works;
        std::vector<std::thread> _threads;
        std::size_t _nextIOService;
    };
} // namespace core
