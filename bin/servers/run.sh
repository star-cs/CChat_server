###
 # @Author: star-cs
 # @Date: 2025-06-22 11:16:49
 # @LastEditTime: 2025-06-22 11:23:34
 # @FilePath: /CChat_server/bin/servers/run.sh
 # @Description: 
### 

#!/bin/bash

# 启动所有子目录中的可执行文件
./ChatServer/ChatServer -f config.ini &
./ChatServer/ChatServer -f config2.ini &
./GateServer/StatusServer -f config.ini &
./StatusServer/StatusServer -f config.ini &
wait  # 等待所有后台任务完成
echo "所有服务已启动"