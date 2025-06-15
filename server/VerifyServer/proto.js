const path = require('path');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');

const PROTO_PATH = path.join(__dirname, 'message.proto');
const packageDefinition = protoLoader.loadSync(PROTO_PATH, {
    keepCase: true,     // 保持字段的大小写。如果为false，则转换为驼峰命名
    longs: String,      // 将64位整数转换为字符串
    enums: String,      // 将枚举转换为字符串
    defaults: true,     // 为未明确设置的字段提供默认值
    oneofs: true,       // 支持 Protobuf 的 oneof 语法
});
const protoDescriptor = grpc.loadPackageDefinition(packageDefinition);

const message_proto = protoDescriptor;

module.exports = message_proto;