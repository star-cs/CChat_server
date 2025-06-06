#pragma once

#include "common.h"
#include "singleton.h"

namespace core
{
    class HttpConnection;
    typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;

    class LogicSystem : public Singleton<LogicSystem>
    {
        friend class Singleton<LogicSystem>;

    public:
        ~LogicSystem();
        bool HandleGet(const std::string& str, std::shared_ptr<HttpConnection> conn);
        // 注册 get 相关的 请求uri，及其对应的 回调函数
        void RegGet(const std::string& str, HttpHandler handler);

        bool HandlePost(const std::string& str, std::shared_ptr<HttpConnection> conn);
        void RegPost(const std::string& str, HttpHandler handler);

    private:
        LogicSystem();
        std::map<std::string, HttpHandler> _post_handlers;
        std::map<std::string, HttpHandler> _get_handlers;
    };
}