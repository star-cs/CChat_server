```
CServer cserver;
   |
std::map<std::string, std::shared_ptr<CSession>> _sessions;
                                         |
                                      _user_id   客户端用户uid
```

# 登录
1. CServer 存储着 CSession_id - CSession 的对应关系。
2. CSession 存储着 用户uid； 
3. Redis存储着 selfserver.name 的连接客户端的数量（为了负载均衡）；
            用户uid TCP所连的ChatServer name（为了找到对端用户所在服务器）；
4. UserMgr 存储着，uid - CSession 的对应关系
