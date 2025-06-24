/*
 * @Author: star-cs
 * @Date: 2025-06-15 22:08:45
 * @LastEditTime: 2025-06-16 09:24:55
 * @FilePath: /CChat_server/ChatServer/src/msg_node.h
 * @Description: 
 */

#pragma once

#include "common.h"

namespace core
{
    class LogicSystem;
    
    class MsgNode
    {
    public:
        /**
         * @param max_len：消息总长度（包括消息头）
         */
        MsgNode(short max_len) : _cur_len(0), _total_len(max_len)
        {
            _data = new char[_total_len + 1]();
            _data[_total_len] = '\0';
        }

        ~MsgNode()
        {
            delete[] _data;
        }
        
        void Clear()
        {
            ::memset(_data, 0, _total_len);
            _cur_len = 0;
        }

        short _cur_len;
        short _total_len;
        char *_data;
    };

    class RecvNode : public MsgNode
    {
        friend class LogicSystem;

    public:
        RecvNode(short max_len, short msg_id);

    private:
        short _msg_id;
    };

    class SendNode : public MsgNode
    {
        friend class LogicSystem;

    public:
        /**
         * @param msg：消息
         * @param max_len: 消息的长度。不包括 HEAD_TOTAL_LEN
         * @param msg_id: 消息类型id
         */
        SendNode(const char *msg, short max_len, short msg_id);

    private:
        short _msg_id;
    };
}