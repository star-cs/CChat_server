#include "logicsystem.h"
#include "httpconnection.h"

namespace core
{
    /**
     * Get
     * /get_test
     * 
     * 
     * 
     * POST
     * /get_verifycode
     * 
     * 
     *  
     */
    LogicSystem::LogicSystem(){
        RegGet("/get_test", [](std::shared_ptr<HttpConnection> connection){
            beast::ostream(connection->_response.body()) << "receive get_test req " << std::endl;
            for(auto& elem : connection->_get_params){
                beast::ostream(connection->_response.body()) << "key=" << elem.first << " value=" << elem.second << "\n";
            }
        });

        RegPost("/get_verifycode", [](std::shared_ptr<HttpConnection> connection){
            auto body_str = beast::buffers_to_string(connection->_request.body().data());
            std::cout << "receive body is " << body_str << std::endl;
            connection->_response.set(http::field::content_type, "text/json");
            Json::Value response_root;
            Json::Reader reader;
            Json::Value ori_root;
            bool parser_success = reader.parse(body_str, ori_root);
            if(!parser_success){
                std::cout << "Failed to parse JSON data!" << std::endl;
                response_root["error"] = ErrorCodes::Error_Json;
                std::string jsonstr = response_root.toStyledString();
                beast::ostream(connection->_response.body()) << jsonstr;
                return true;
            }
            
            if(!ori_root.isMember("email")){    // 没有email
                std::cout << "Failed to parse JSON data!" << std::endl;
                response_root["error"] = ErrorCodes::Error_Json;
                std::string jsonstr = response_root.toStyledString();
                beast::ostream(connection->_response.body()) << jsonstr;
                return true;
            }

            auto email = ori_root["email"].asString();
            std::cout << "email is " << email << std::endl;
            
            response_root["error"] = ErrorCodes::Success;
            response_root["email"] = ori_root["email"];
            std::string jsonstr = response_root.toStyledString();
            beast::ostream(connection->_response.body()) << jsonstr;
            return true;
        });
    }

    LogicSystem::~LogicSystem(){

    }

    bool LogicSystem::HandleGet(const std::string& str, std::shared_ptr<HttpConnection> conn){
        if(_get_handlers.find(str) == _get_handlers.end()){
            return false;
        }
        _get_handlers[str](conn);
        return true;
    }

    void LogicSystem::RegGet(const std::string& str, HttpHandler handler){
        _get_handlers.insert(make_pair(str, handler));
    }

    bool LogicSystem::HandlePost(const std::string& str, std::shared_ptr<HttpConnection> conn){
        if(_post_handlers.find(str) == _post_handlers.end()){
            return false;
        }
        _post_handlers[str](conn);
        return true;
    }

    void LogicSystem::RegPost(const std::string& str, HttpHandler handler){
        _post_handlers.insert(make_pair(str, handler));
    }

}