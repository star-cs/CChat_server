/*
 * @Author: star-cs
 * @Date: 2025-06-07 15:11:02
 * @LastEditTime: 2025-06-07 17:08:24
 * @FilePath: /CChat_server/VerifyServer/src/proto.js
 * @Description: 解析 proto 文件
 */

// 模块导入
const path = require('path')                        // nodejs核心，处理文件路径
const grpc = require('@grpc/grpc-js')               // gRPC
const protoLoader = require('@grpc/proto-loader')   // proto文件加载器

const PROTO_PATH = path.join(__dirname, '../proto/message.proto')

// 同步加载 proto 文件      异步版本load()
const packageDefinition = protoLoader.loadSync(PROTO_PATH, {
    keepCase: true, // true为字段名保持原样，false为转成驼峰命名法
    longs: String,  // 64位整数 --> string，避免 js 中整数溢出问题
    enums: String,  // 枚举 --> string
    defaults: true, // 其余默认
    oneofs: true    // 启用 oneof 特性（互斥字段）
})

// loadPackageDefinition() 将 proto 定义转化为 gRPC 可用对象
// protoDescriptor 包含 proto 文件中定义的所有服务和消息类型
const protoDescriptor = grpc.loadPackageDefinition(packageDefinition)

// 获取特定服务
// message 对于 proto 文件中的 package 字段
const message_proto = protoDescriptor.message

// 导出模块
// 其他文件可通过 require() 导入该 gRPC 定义
module.exports = message_proto

/**
 * gRPC 工作流程：
 * .proto文件 -> protoLoader 解析 -> gRPC 包定义 -> 创建服务端/客户端
 * 
 */