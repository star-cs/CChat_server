/*
 * @Author: star-cs
 * @Date: 2025-06-30 11:24:36
 * @LastEditTime: 2025-06-30 19:25:57
 * @FilePath: /CChat_server/Common/src/singleton.h
 * @Description: 
 */
#pragma once

#include <iostream>
#include <memory>
#include <mutex>

namespace core
{
template <typename T>
class Singleton
{
protected:
    Singleton() = default;
    Singleton(const Singleton<T> &) = delete;
    Singleton &operator=(const Singleton<T> &) = delete;

    static std::shared_ptr<T> _instance;

public:
    static std::shared_ptr<T> GetInstance()
    {
        // 保证一次构造
        static std::once_flag s_flag;
        std::call_once(s_flag, [&]() { _instance = std::shared_ptr<T>(new T); });

        return _instance;
    }

    void PrintAddress() { std::cout << _instance.get() << std::endl; }
    ~Singleton() { std::cout << "this is singleton destruct" << std::endl; }
};

template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
} // namespace core
