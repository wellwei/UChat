# UChat Server

`UChat` 是一个面向实时通讯场景的分布式 IM 服务端示例项目。当前服务端由 `GateServer`、`ChatServer`、`UserServer`、`MQGateway` 等组件组成，采用 gRPC 进行服务间通信，Redis 保存短期离线消息和在线状态，MySQL 保存用户与群信息。

项目当前目标不是做完整商用 IM，而是完成一个结构清晰、可演示、可压测、支持水平扩展的实时通讯后端。

## 当前能力

- 单聊消息发送、在线推送、离线同步
- 客户端基于 `last_inbox_seq` 的增量同步
- 联系人申请、处理、联系人列表查询
- 群聊基础能力
  - 创建群
  - 修改群
  - 删除群
  - 搜索群
  - 我的群列表
  - 加入群
  - 退出群
  - 查询群成员
  - 小群消息发送
- 服务端幂等发送
- Redis 短期离线缓存
- Nginx 逻辑负载均衡下的多实例部署

## 当前边界

- 服务端不做消息历史入库
- 消息历史由客户端本地持久化
- Redis 只保存短期离线消息
- 群聊仅定位为小群
  当前实现建议群规模不超过 `100`
- 群消息当前采用同步 fanout
  适合简历项目和小群场景
- `GetVerifyCode` 目前未实现

## 服务架构

### GateServer

对客户端暴露统一入口。

- 提供双向流 `ChatTunnel`
- 提供一元 RPC `ChatControlApi`
- 负责连接管理、JWT 登录校验、在线会话管理
- 将消息发送、同步、ACK 转发给 `ChatServer`
- 将用户、联系人、群操作转发给 `UserServer`

### ChatServer

负责实时消息链路。

- 单聊发送
- 小群发送
- Redis 离线收件箱写入
- 离线消息同步
- 在线消息推送到 `GateServer`

`ChatServer` 设计上保持无状态，可水平扩展。核心状态不保存在本机内存：

- 在线状态在 Redis
- 离线消息在 Redis
- 群成员和群资料在 `UserServer` / MySQL

### UserServer

负责用户域和关系域数据。

- 用户注册、登录校验、资料查询修改
- 联系人相关操作
- 群聊信息和群成员管理
- 群成员查询

`UserServer` 也应通过 Nginx 或服务治理层暴露逻辑服务地址，不依赖单机节点。

### Redis

当前承担两类核心状态：

- 在线状态
- 短期离线消息

消息同步采用“用户收件箱时间线”模型，而不是会话级游标。

主要 key 设计：

- `user_inbox_seq:{uid}`
- `offline_index:{uid}`
- `offline_msg:{uid}:{seq}`
- `ack_seq:{uid}`
- `online:{uid}`
- 幂等键

### MySQL

保存用户、联系人、群、群成员等结构化数据。

当前群表：

- `chat_groups`
- `group_members`

## 核心消息模型

### 1. Envelope

所有消息在客户端和服务间统一使用 `Envelope`：

- `msg_id`
  服务端生成，全局唯一
- `client_msg_id`
  客户端生成，用于幂等
- `from_uid`
- `to_uid`
- `group_id`
  为 `0` 表示单聊
- `ts_ms`
  服务端时间戳
- `payload`
  业务负载，二进制透传
- `trace_id`
  调试和链路追踪可选
- `seq_id`
  接收者视角的 `inbox_seq`

### 2. 单聊同步模型

服务端不再要求客户端上传会话级 `last_seq_map`，客户端只维护一个连续的 `last_inbox_seq`。

同步逻辑：

1. 客户端上传 `last_inbox_seq`
2. 服务端返回 `seq_id > last_inbox_seq` 的消息
3. 客户端更新本地最大连续 `seq`
4. 客户端后续继续用新的 `last_inbox_seq` 调 `Sync`

### 3. 群消息模型

群消息采用“小群同步 fanout”：

1. `ChatServer` 查询群成员
2. 为每个接收成员分配自己的 `inbox_seq`
3. 分别写入该成员的离线收件箱
4. 在线则直接推送

也就是说，同一条群消息对不同成员会对应不同的 `seq_id`。

## 认证模型

### 流式连接

客户端连接 `ChatTunnel` 后，首先发送：

- `LoginReq.uid`
- `LoginReq.token`
- `LoginReq.device_id`

`GateServer` 会校验 JWT，成功后建立在线会话。

### 一元 RPC

`ChatControlApi` 中除登录注册类接口外，其他接口都要求客户端带上 gRPC metadata：

- `uid`
- `token`

当前代码中，服务端处理逻辑实际强依赖 `uid` metadata；`token` 在一元接口侧建议一并透传，便于后续统一鉴权扩展。

## 推荐部署方式

### 逻辑服务入口

建议所有服务都通过 Nginx 或统一逻辑服务地址访问：

- `GateServer` 由客户端访问
- `ChatServer` 由 `GateServer` 访问
- `UserServer` 由 `GateServer`、`ChatServer` 访问

即使只有单实例，也建议保持“逻辑地址”配置方式，避免后续扩容时改代码。

### 推荐拓扑

```text
Client
  -> Nginx(GateServer upstream)
  -> GateServer
       -> Nginx(ChatServer upstream)
       -> ChatServer
       -> Nginx(UserServer upstream)
       -> UserServer

ChatServer / GateServer / UserServer
  -> Redis
UserServer
  -> MySQL
```

## 构建

各服务分别构建：

- [ChatServer](/Users/goodnight/Projects/cxx/UChat/server/ChatServer)
- [GateServer](/Users/goodnight/Projects/cxx/UChat/server/GateServer)
- [UserServer](/Users/goodnight/Projects/cxx/UChat/server/UserServer)

典型命令：

```bash
cd ChatServer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

其余服务类似。

## 配置

每个服务使用自己的 `config.ini`。

重点配置项：

- `GateServer/config.ini`
  - `ChatServer.host/port`
  - `UserServer.host/port`
  - `Redis.host/port`
- `ChatServer/config.ini`
  - `UserServer.host/port`
  - `Redis.host/port`
- `UserServer/config.ini`
  - `Mysql`
  - `Redis`

如果前面挂了 Nginx，上述地址应配置为逻辑服务入口，而不是单台实例。

## 数据库初始化

执行 [schema.sql](/Users/goodnight/Projects/cxx/UChat/server/schema.sql) 初始化表结构。

当前包含：

- 用户表
- 联系人申请表
- 联系人关系表
- 群表
- 群成员表

## 测试与压测

`ChatServer` 侧已包含集成测试和压测程序，重点覆盖：

- 单聊发送
- 离线同步
- 幂等发送
- 分页同步
- ACK 语义
- 发送与同步 QPS 测试

在 `Release` 模式下，当前瓶颈主要体现在 Redis，而不是 C++ 服务本身。

## 客户端对接文档

客户端开发请直接参考：

[docs/CLIENT_API.md](/Users/goodnight/Projects/cxx/UChat/server/docs/CLIENT_API.md)

该文档包含：

- 流式通道协议
- 一元控制接口
- 鉴权和 metadata 规范
- 单聊 / 群聊消息发送规范
- Sync 断点续传模型
- 客户端推荐实现方式

## 当前实现建议

为了尽快完成项目，建议客户端优先按以下顺序对接：

1. 注册 / 登录
2. 建立 `ChatTunnel`
3. 单聊发送与接收
4. `Sync` 增量同步
5. 联系人模块
6. 群 CRUD 和群成员查询
7. 小群消息

## 后续可扩展方向

- 群成员 Redis 共享缓存
- 大群异步 fanout worker
- 服务端统一鉴权拦截器
- 消息已读回执
- 多端同步状态
- 推送失败重试和批量投递优化
