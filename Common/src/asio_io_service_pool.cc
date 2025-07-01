/*
 * @Author: star-cs
 * @Date: 2025-06-30 11:24:36
 * @LastEditTime: 2025-06-30 22:23:06
 * @FilePath: /CChat_server/Common/src/asio_io_service_pool.cc
 * @Description: 
 */
#include "asio_io_service_pool.h"
#include "configmgr.h"

namespace core
{
AsioIOServicePool::AsioIOServicePool(std::size_t size) : _nextIOService(0), _stopped(false)
{
    std::string threadNum = ConfigMgr::GetInstance()["IOServicePool"]["threadNum"];
    std::size_t ioServicePoolnum = boost::lexical_cast<std::size_t>(threadNum);
    if (ioServicePoolnum < 1 || ioServicePoolnum > 64) {
        // throw std::out_of_range("Invalid threadNum value: " + threadNum);
        LOG_INFO("config.ini Invalid threadNum value: {}", threadNum);
        ioServicePoolnum = size;
    }
    LOG_INFO("AsioIOServicePool ThreadNum value: {}", ioServicePoolnum);
    for (std::size_t i = 0; i < ioServicePoolnum; ++i) {
        IOServicePtr ios = std::make_unique<IOService>();
        WorkPtr work = std::make_unique<Work>(ios->get_executor());
        _ioServices.push_back(std::move(ios));
        _works.push_back(std::move(work));
    }

    for (size_t i = 0; i < _ioServices.size(); ++i) {
        _threads.emplace_back([this, i]() { _ioServices[i]->run(); });
    }
}

AsioIOServicePool::~AsioIOServicePool()
{
    std::cout << "AsioIOServicePool destruct" << std::endl;
    Stop();
}

boost::asio::io_context &AsioIOServicePool::GetIOService()
{
    boost::asio::io_context &service = *_ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size()) {
        _nextIOService = 0;
    }
    return service;
}

void AsioIOServicePool::Stop()
{
    if(_stopped){
        return;
    }
    _stopped = true;
    //因为仅仅执行work.reset并不能让iocontext从run的状态中退出
    //当iocontext已经绑定了读或写的监听事件后，还需要手动stop该服务。
    for (auto &work : _works) {
        //把服务先停止
        work->get_executor().context().stop();
        work.reset();
    }
    // 等待所有线程完成
    for (auto &t : _threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}
} // namespace core