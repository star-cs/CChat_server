#pragma once
#include <iostream>
#include <memory>
#include <string>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#define SOURCE_DIR "@PROJECT_SOURCE_DIR@"
#define LOG_LEVEL info
#define LOGTOFILE

namespace coro::config
{
    constexpr char kLogFileName[] = "/logs/GateServer.log";
    constexpr int kFlushDura = 3; // 刷新间隔，单位：秒
}

namespace coro::log
{
#define CONFIG_LOG_LEVEL(log_level) spdlog::level::log_level

    using std::make_shared;
    using std::shared_ptr;
    using std::string;
    using spdlogger = shared_ptr<spdlog::logger>;

    class logger
    {
    public:
#ifdef LOGTOFILE
        static auto get_logger() noexcept -> spdlogger &
        {
            static logger log;
            return log.m_logger;
        };
#endif

        logger(const logger &) = delete;
        logger(logger &&) = delete;
        auto operator=(const logger &) -> logger & = delete;
        auto operator=(logger &&) -> logger & = delete;

    private:
        logger() noexcept
        {
#ifdef LOGTOFILE
            string log_path = string(SOURCE_DIR) + string(coro::config::kLogFileName);
            m_logger = spdlog::create<spdlog::sinks::basic_file_sink_mt>("corolog", log_path.c_str(), true);
            m_logger->set_pattern("[%n][%Y-%m-%d %H:%M:%S] [%l] [%t] %v");
            m_logger->set_level(CONFIG_LOG_LEVEL(LOG_LEVEL));
            spdlog::flush_every(std::chrono::seconds(coro::config::kFlushDura));
#endif
        }

        ~logger() noexcept
        {
#ifdef LOGTOFILE
            m_logger->flush_on(CONFIG_LOG_LEVEL(LOG_LEVEL));
#endif
        }

    private:
#ifdef LOGTOFILE
        spdlogger m_logger;
#endif
    };

    template <typename... T>
    inline void trace(const char *__restrict__ fmt, const T &...args)
    {
        if constexpr ((CONFIG_LOG_LEVEL(LOG_LEVEL)) <= spdlog::level::trace)
        {
#ifdef LOGTOFILE
            logger::get_logger()->trace(fmt, args...);
#endif
            spdlog::trace(fmt, args...);
        }
    }

    template <typename... T>
    inline void debug(const char *__restrict__ fmt, const T &...args)
    {
        if constexpr ((CONFIG_LOG_LEVEL(LOG_LEVEL)) <= spdlog::level::debug)
        {
#ifdef LOGTOFILE
            logger::get_logger()->debug(fmt, args...);
#endif
            spdlog::debug(fmt, args...); 
        }
    }

    template <typename... T>
    inline void info(const char *__restrict__ fmt, const T &...args)
    {
        if constexpr ((CONFIG_LOG_LEVEL(LOG_LEVEL)) <= spdlog::level::info)
        {
#ifdef LOGTOFILE
            logger::get_logger()->info(fmt, args...);
#endif
            spdlog::info(fmt, args...);
        }
    }

    template <typename... T>
    inline void warn(const char *__restrict__ fmt, const T &...args)
    {
        if constexpr ((CONFIG_LOG_LEVEL(LOG_LEVEL)) <= spdlog::level::warn)
        {
#ifdef LOGTOFILE
            logger::get_logger()->warn(fmt, args...);
#endif
            spdlog::warn(fmt, args...);
        }
    }

    template <typename... T>
    inline void error(const char *__restrict__ fmt, const T &...args)
    {
        if constexpr ((CONFIG_LOG_LEVEL(LOG_LEVEL)) <= spdlog::level::err)
        {
#ifdef LOGTOFILE
            logger::get_logger()->error(fmt, args...);
#endif
            spdlog::error(fmt, args...);
        }
    }

    template <typename... T>
    inline void critical(const char *__restrict__ fmt, const T &...args)
    {
        if constexpr ((CONFIG_LOG_LEVEL(LOG_LEVEL)) <= spdlog::level::critical)
        {
#ifdef LOGTOFILE
            logger::get_logger()->critical(fmt, args...);
#endif
            spdlog::critical(fmt, args...);
        }
    }

}; // namespace coro::log

// 添加简洁的日志宏定义
#define LOG_TRACE(fmt, ...) coro::log::trace("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) coro::log::debug("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  coro::log::info("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  coro::log::warn("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) coro::log::error("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_CRITICAL(fmt,...) coro::log::critical("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

