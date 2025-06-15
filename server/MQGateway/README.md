 # MQGateway - 消息队列网关服务

MQGateway 是 UChat 聊天系统的消息队列网关服务，基于 Go 语言开发，使用 RabbitMQ 作为消息中间件，对外提供 gRPC 服务接口。

## 功能特性

- **消息发布**: 支持单条和批量消息发布到 RabbitMQ
- **消息订阅**: 支持流式消息订阅，实时接收消息通知
- **路由管理**: 基于 topic 交换机的灵活路由配置
- **健康检查**: 提供服务和 RabbitMQ 连接状态检查
- **自动重连**: RabbitMQ 连接断开时自动重连
- **优雅关闭**: 支持优雅关闭，确保消息不丢失

## 架构设计

### 整体架构

```
┌─────────────────┐    gRPC     ┌─────────────────┐    AMQP     ┌─────────────────┐
│   UserServer    │ ──────────► │   MQGateway     │ ──────────► │   RabbitMQ      │
│   ChatServer    │             │                 │             │                 │
│   StatusServer  │             │                 │             │                 │
└─────────────────┘             └─────────────────┘             └─────────────────┘
```

### 消息流程

1. **发布消息**: 各个微服务通过 gRPC 调用 MQGateway 发布消息
2. **路由分发**: MQGateway 将消息发布到 RabbitMQ 的 topic 交换机
3. **消息消费**: ChatServer 等服务订阅相关队列，接收消息通知
4. **实时推送**: ChatServer 将消息推送给在线用户

### 消息类型

- `CONTACT_REQUEST`: 联系人请求
- `CONTACT_ACCEPTED`: 联系人请求被接受
- `CONTACT_REJECTED`: 联系人请求被拒绝
- `CHAT_MESSAGE`: 聊天消息
- `SYSTEM_NOTIFICATION`: 系统通知

### 路由键设计

- `contact.request.{user_id}`: 联系人请求通知
- `contact.accept.{user_id}`: 联系人接受通知
- `contact.reject.{user_id}`: 联系人拒绝通知
- `message.{user_id}`: 聊天消息通知
- `system.{user_id}`: 系统通知

## 快速开始

### 前置要求

- Go 1.21+
- RabbitMQ 3.8+
- Protocol Buffers 编译器

### 安装依赖

```bash
# macOS
make install-protoc-mac

# Ubuntu
make install-protoc-ubuntu

# 安装 Go 依赖
make deps
```

### 构建和运行

```bash
# 生成 protobuf 代码
make proto

# 构建
make build

# 运行
make run

# 或者开发模式运行
make dev
```

### 配置文件

编辑 `config.yaml` 文件：

```yaml
server:
  host: "0.0.0.0"
  port: 50055

rabbitmq:
  url: "amqp://guest:guest@localhost:5672/"
  exchange: "uchat_exchange"
  exchange_type: "topic"
  
  queues:
    contact_request_notifications: "contact.request.notifications"
    contact_accept_notifications: "contact.accept.notifications"
    message_notifications: "message.notifications"

logging:
  level: "info"
  file: "logs/mqgateway.log"
```

## API 接口

### gRPC 服务

#### PublishMessage - 发布单条消息

```protobuf
rpc PublishMessage(PublishMessageRequest) returns (PublishMessageResponse);
```

#### BatchPublish - 批量发布消息

```protobuf
rpc BatchPublish(BatchPublishRequest) returns (BatchPublishResponse);
```

#### Subscribe - 订阅消息

```protobuf
rpc Subscribe(SubscribeRequest) returns (stream MessageNotification);
```

#### HealthCheck - 健康检查

```protobuf
rpc HealthCheck(HealthCheckRequest) returns (HealthCheckResponse);
```

### 使用示例

#### 发布联系人请求消息

```go
client := pb.NewMQGatewayServiceClient(conn)

req := &pb.PublishMessageRequest{
    RoutingKey:   "contact.request.123",
    MessageType:  pb.MessageType_CONTACT_REQUEST,
    Payload:      `{"request_id": 456, "requester_name": "Alice"}`,
    TargetUserId: 123,
    SenderUserId: 456,
    Timestamp:    time.Now().Unix(),
}

resp, err := client.PublishMessage(context.Background(), req)
```

#### 订阅消息

```go
stream, err := client.Subscribe(context.Background(), &pb.SubscribeRequest{
    QueueName:    "contact_notifications_123",
    RoutingKeys:  []string{"contact.*.123"},
})

for {
    notification, err := stream.Recv()
    if err != nil {
        break
    }
    // 处理消息通知
    fmt.Printf("收到消息: %s\n", notification.Payload)
}
```

## 集成到 UChat 系统

### UserServer 集成

在 UserServer 中，当处理联系人请求时：

```cpp
// 发送联系人请求后，通知目标用户
void UserServer::SendContactRequest(...) {
    // ... 数据库操作 ...
    
    // 通过 MQGateway 发送通知
    auto mqClient = MQGatewayClient::getInstance();
    mqClient->publishContactRequestNotification(addressee_id, request_info);
}
```

### ChatServer 集成

ChatServer 订阅相关消息队列：

```cpp
// ChatServer 启动时订阅消息
void ChatServer::start() {
    auto mqClient = MQGatewayClient::getInstance();
    mqClient->subscribe("contact_notifications", {"contact.*"}, 
        [this](const MessageNotification& notification) {
            this->handleContactNotification(notification);
        });
}
```

## 监控和运维

### 健康检查

```bash
# 使用 grpcurl 检查服务状态
grpcurl -plaintext localhost:50055 mqgateway.MQGatewayService/HealthCheck
```

### 日志监控

```bash
# 查看日志
tail -f logs/mqgateway.log
```

### RabbitMQ 管理

```bash
# 查看队列状态
rabbitmqctl list_queues

# 查看交换机
rabbitmqctl list_exchanges

# 查看绑定关系
rabbitmqctl list_bindings
```

## 性能优化

### 连接池配置

- 调整 RabbitMQ 连接参数
- 配置合适的预取数量
- 设置消息确认模式

### 消息批处理

- 使用 BatchPublish 接口减少网络开销
- 配置合适的批处理大小和超时时间

### 内存管理

- 监控消息队列长度
- 设置合适的消息 TTL
- 配置死信队列处理异常消息

## 故障排除

### 常见问题

1. **RabbitMQ 连接失败**
   - 检查 RabbitMQ 服务状态
   - 验证连接 URL 和认证信息
   - 检查网络连通性

2. **消息丢失**
   - 确认消息持久化配置
   - 检查消息确认机制
   - 验证队列持久化设置

3. **性能问题**
   - 监控队列积压情况
   - 调整消费者数量
   - 优化消息序列化

### 调试工具

```bash
# 使用 grpcurl 测试接口
grpcurl -plaintext -d '{"routing_key":"test","message_type":"CONTACT_REQUEST","payload":"{}"}' \
    localhost:50055 mqgateway.MQGatewayService/PublishMessage

# 查看 RabbitMQ 状态
rabbitmq-diagnostics status
```

## 开发指南

### 添加新的消息类型

1. 在 `proto/mqgateway.proto` 中添加新的 MessageType
2. 更新 `internal/service/mqgateway.go` 中的类型转换逻辑
3. 重新生成 protobuf 代码：`make proto`

### 扩展路由规则

1. 在 `config.yaml` 中添加新的队列配置
2. 更新相关服务的订阅逻辑
3. 测试新的路由规则

## 许可证

本项目采用 MIT 许可证。