/*
 * @Author: star-cs
 * @Date: 2025-06-15 22:15:24
 * @LastEditTime: 2025-06-16 17:05:16
 * @FilePath: /CChat_server/ChatServer/src/msg_node.cc
 * @Description: 
 */
#include "msg_node.h"


namespace core
{
    RecvNode::RecvNode(short max_len, short msg_id): MsgNode(max_len), _msg_id(msg_id){

    }

    SendNode::SendNode(const char *msg, short max_len, short msg_id): MsgNode(max_len + HEAD_TOTAL_LEN), _msg_id(msg_id){
        // msg_id 转为网络字节序
        short msg_id_host = boost::asio::detail::socket_ops::host_to_network_short(_msg_id);
        ::memcpy(_data, &msg_id_host, HEAD_ID_LEN);
        short max_len_host =  boost::asio::detail::socket_ops::host_to_network_short(max_len);
        ::memcpy(_data + HEAD_ID_LEN, &max_len_host, HEAD_DATA_LEN);
        // ::memcpy(_data + HEAD_TOTAL_LEN, &msg, max_len);
        ::memcpy(_data + HEAD_TOTAL_LEN, msg, max_len);
    }   

}