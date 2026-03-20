# UChat Client API

本文档面向客户端开发，约定客户端只直接访问 `GateServer` 暴露的接口：

- `ChatTunnel.Connect`
- `ChatControlApi.*`

内部服务 `ChatServer`、`UserServer` 为服务端内部实现细节，客户端无需直接调用。

## 1. 服务入口

客户端应连接 `GateServer` 的逻辑服务地址。

建议：

- Web / Mobile / Desktop 都统一连接 Nginx 暴露的 `GateServer` upstream
- 不直接写死某台 `GateServer` 节点地址

## 2. 通信模型

### 2.1 双向流 `ChatTunnel`

用途：

- 登录建链
- 心跳
- 发消息
- 接收服务端推送
- 流式 ACK

### 2.2 一元 RPC `ChatControlApi`

用途：

- 注册、登录
- 用户资料
- 联系人
- 群聊管理
- 离线消息同步

## 3. 认证约定

### 3.1 登录态建立

流式连接成功后，客户端第一帧发送 `LoginReq`：

```protobuf
message LoginReq {
  int64 uid = 1;
  string token = 2;
  string device_id = 3;
}
```

登录成功返回：

```protobuf
message LoginResp {
  bool ok = 1;
  string reason = 2;
  int64 session_ver = 3;
}
```

说明：

- `uid` 与 `token` 必须匹配
- `token` 由 `AuthLogin` 返回
- `device_id` 由客户端自己生成并稳定保存

### 3.2 一元 RPC 认证 metadata

除注册、登录、重置密码等无需登录态接口外，其余接口都应带 gRPC metadata：

- `uid`
- `token`

当前服务端代码实际依赖 `uid` metadata 识别操作者身份。客户端实现时应始终同时透传 `uid` 和 `token`。

示例：

```text
metadata:
  uid: "10001"
  token: "jwt-token-string"
```

## 4. 公共消息结构

```protobuf
message Envelope {
  string msg_id = 1;
  string client_msg_id = 2;
  int64  from_uid = 3;
  int64  to_uid = 4;
  int64  group_id = 5;
  int64  ts_ms = 6;
  bytes  payload = 7;
  string trace_id = 8;
  uint64 seq_id = 9;
}
```

字段语义：

- `msg_id`
  服务端生成的全局唯一消息 ID
- `client_msg_id`
  客户端生成的幂等 ID，建议使用 UUID
- `from_uid`
  发送者
- `to_uid`
  单聊目标用户
- `group_id`
  群 ID
  `0` 表示单聊
- `payload`
  业务消息体，二进制透传
- `seq_id`
  接收者视角的消息序号
  客户端增量同步依赖这个字段

### 单聊消息约束

- `group_id = 0`
- `to_uid > 0`

### 群聊消息约束

- `group_id > 0`
- `to_uid` 可忽略

## 5. ChatTunnel 详细说明

### 5.1 ClientFrame

```protobuf
message ClientFrame {
  oneof body {
    LoginReq    login = 1;
    Heartbeat   hb    = 2;
    SendReq     send  = 3;
    AckReq      ack   = 4;
  }
}
```

### 5.2 ServerFrame

```protobuf
message ServerFrame {
  oneof body {
    LoginResp   login = 1;
    Pong        pong  = 2;
    PushMsg     push  = 3;
    SendResp    send  = 4;
    Kick        kick  = 5;
    Error       err   = 6;
  }
}
```

### 5.3 心跳

客户端发送：

```protobuf
message Heartbeat { int64 ts_ms = 1; }
```

服务端返回：

```protobuf
message Pong { int64 ts_ms = 1; }
```

建议：

- 心跳间隔 `20s ~ 30s`
- 断线后自动重连
- 重连后立即执行 `Sync`

### 5.4 发消息

客户端发送：

```protobuf
message SendReq { Envelope env = 1; }
```

服务端返回：

```protobuf
message SendResp {
  bool ok = 1;
  string msg_id = 2;
  string reason = 3;
}
```

客户端发送单聊示例：

```text
env:
  client_msg_id = uuid
  from_uid = 当前登录用户
  to_uid = 目标 uid
  group_id = 0
  payload = 二进制消息体
  trace_id = 可选
```

客户端发送群聊示例：

```text
env:
  client_msg_id = uuid
  from_uid = 当前登录用户
  group_id = 群 id
  payload = 二进制消息体
  trace_id = 可选
```

说明：

- `msg_id` 只有服务端成功接收后才会返回
- 使用同一个 `client_msg_id` 重试时，服务端会做幂等处理

### 5.5 推送消息

服务端通过：

```protobuf
message PushMsg { Envelope env = 1; }
```

推送给客户端。

客户端收到后建议流程：

1. 校验消息结构
2. 用 `msg_id` 去重
3. 按 `seq_id` 更新本地连续游标
4. 写入本地消息存储
5. 回发 `AckReq`

### 5.6 ACK

```protobuf
message AckReq {
  string msg_id = 1;
  AckType type = 2;
  int64 ts_ms = 3;
  uint64 max_inbox_seq = 4;
}
```

当前推荐客户端发送：

- `type = ACK_RECEIVED`
- `msg_id = 收到的消息 ID`
- `max_inbox_seq = 当前本地已连续处理到的最大 seq`

说明：

- 当前服务端 ACK 主要作为轻量确认，不会强制实时删除 Redis 离线数据
- 真正避免重复同步的关键是客户端维护 `last_inbox_seq`

## 6. ChatControlApi 详细说明

## 6.1 Sync

用于断线重连或补拉漏消息。

请求：

```protobuf
message SyncReq {
  uint64 last_inbox_seq = 1;
  uint32 limit = 2;
}
```

响应：

```protobuf
message SyncResp {
  bool ok = 1;
  string message = 2;
  repeated Envelope msgs = 3;
  uint64 max_inbox_seq = 4;
  bool has_more = 5;
}
```

字段说明：

- `last_inbox_seq`
  客户端本地已连续持久化的最大 `seq`
- `limit`
  单次拉取条数，建议 `100~200`
- `max_inbox_seq`
  当前返回批次里的最大 `seq`
- `has_more`
  是否还有更多未拉取消息

推荐客户端逻辑：

1. 本地维护 `last_inbox_seq`
2. 调 `Sync(last_inbox_seq, limit)`
3. 按顺序写入 `msgs`
4. 更新本地 `last_inbox_seq`
5. 若 `has_more = true`，继续拉下一页

## 6.2 Register

请求：

```protobuf
message RegisterReq {
  string email = 1;
  string verify_code = 2;
  string username = 3;
  string password = 4;
}
```

响应：

```protobuf
message RegisterResp {
  bool ok = 1;
  string message = 2;
  int64 uid = 3;
}
```

说明：

- 当前 `verify_code` 字段保留，但配套校验流程尚未完整接通

## 6.3 AuthLogin

请求：

```protobuf
message AuthLoginReq {
  string handle = 1;
  string password = 2;
}
```

响应：

```protobuf
message AuthLoginResp {
  bool ok = 1;
  string message = 2;
  int64 uid = 3;
  string token = 4;
}
```

说明：

- `handle` 可以是用户名或邮箱
- 成功后客户端保存 `uid` 和 `token`
- 后续流式登录和一元 RPC metadata 都依赖这两个值

## 6.4 ResetPassword

请求：

```protobuf
message ResetPasswordReq {
  string email = 1;
  string verify_code = 2;
  string new_password = 3;
}
```

响应：

```protobuf
message ResetPasswordResp {
  bool ok = 1;
  string message = 2;
}
```

## 6.5 GetVerifyCode

当前未实现。

客户端不要依赖该接口。

## 7. 用户资料接口

## 7.1 GetUserProfile

请求：

```protobuf
message GetUserProfileReq {
  int64 target_uid = 1;
}
```

说明：

- `target_uid = 0` 表示查询自己

响应：

```protobuf
message GetUserProfileResp {
  bool ok = 1;
  string message = 2;
  ProfileData profile = 3;
}
```

## 7.2 UpdateUserProfile

请求：

```protobuf
message UpdateUserProfileReq {
  string nickname = 1;
  string avatar_url = 2;
  string phone = 3;
  int32 gender = 4;
  string signature = 5;
  string location = 6;
}
```

响应：

```protobuf
message UpdateUserProfileResp {
  bool ok = 1;
  string message = 2;
  ProfileData profile = 3;
}
```

## 7.3 SearchUser

请求：

```protobuf
message SearchUserReq {
  string keyword = 1;
  int32 limit = 2;
}
```

响应：

```protobuf
message SearchUserResp {
  bool ok = 1;
  string message = 2;
  repeated ProfileData profiles = 3;
}
```

## 8. 联系人接口

## 8.1 GetContacts

请求：

```protobuf
message GetContactsReq {}
```

响应：

```protobuf
message GetContactsResp {
  bool ok = 1;
  string message = 2;
  repeated ContactEntryData contacts = 3;
}
```

## 8.2 GetContactRequests

请求：

```protobuf
message GetContactRequestsReq {}
```

响应：

```protobuf
message GetContactRequestsResp {
  bool ok = 1;
  string message = 2;
  repeated ContactRequestData requests = 3;
}
```

## 8.3 SendContactRequest

请求：

```protobuf
message SendContactReqReq {
  int64 to_uid = 1;
  string note = 2;
}
```

响应：

```protobuf
message SendContactReqResp {
  bool ok = 1;
  string message = 2;
}
```

## 8.4 HandleContactRequest

请求：

```protobuf
message HandleContactReqReq {
  int64 request_id = 1;
  bool accept = 2;
}
```

响应：

```protobuf
message HandleContactReqResp {
  bool ok = 1;
  string message = 2;
}
```

## 9. 群聊管理接口

当前群聊定位为小群。

建议限制：

- 群规模不超过 `100`

## 9.1 CreateGroup

请求：

```protobuf
message CreateGroupReq {
  string name = 1;
  string avatar_url = 2;
  string notice = 3;
  string description = 4;
  repeated int64 member_uids = 5;
}
```

说明：

- 当前登录用户会自动成为 owner
- `member_uids` 是创建时一起拉入的初始成员列表

响应：

```protobuf
message CreateGroupResp {
  bool ok = 1;
  string message = 2;
  GroupData group = 3;
}
```

## 9.2 UpdateGroup

请求：

```protobuf
message UpdateGroupReq {
  int64 group_id = 1;
  string name = 2;
  string avatar_url = 3;
  string notice = 4;
  string description = 5;
}
```

响应：

```protobuf
message UpdateGroupResp {
  bool ok = 1;
  string message = 2;
  GroupData group = 3;
}
```

说明：

- 当前仅群主可修改

## 9.3 DeleteGroup

请求：

```protobuf
message DeleteGroupReq {
  int64 group_id = 1;
}
```

响应：

```protobuf
message DeleteGroupResp {
  bool ok = 1;
  string message = 2;
}
```

说明：

- 当前仅群主可删除

## 9.4 GetGroup

请求：

```protobuf
message GetGroupReq {
  int64 group_id = 1;
}
```

响应：

```protobuf
message GetGroupResp {
  bool ok = 1;
  string message = 2;
  GroupData group = 3;
}
```

## 9.5 SearchGroups

请求：

```protobuf
message SearchGroupsReq {
  string keyword = 1;
  uint32 limit = 2;
}
```

响应：

```protobuf
message SearchGroupsResp {
  bool ok = 1;
  string message = 2;
  repeated GroupData groups = 3;
}
```

## 9.6 ListMyGroups

请求：

```protobuf
message ListMyGroupsReq {}
```

响应：

```protobuf
message ListMyGroupsResp {
  bool ok = 1;
  string message = 2;
  repeated GroupData groups = 3;
}
```

## 9.7 JoinGroup

请求：

```protobuf
message JoinGroupReq {
  int64 group_id = 1;
}
```

响应：

```protobuf
message JoinGroupResp {
  bool ok = 1;
  string message = 2;
}
```

说明：

- 当前实现为直接加入
- 没有审核流

## 9.8 QuitGroup

请求：

```protobuf
message QuitGroupReq {
  int64 group_id = 1;
}
```

响应：

```protobuf
message QuitGroupResp {
  bool ok = 1;
  string message = 2;
}
```

说明：

- 群主当前不能直接退群，应先删除群或后续扩展转让群主

## 9.9 GetGroupMembers

请求：

```protobuf
message GetGroupMembersReq {
  int64 group_id = 1;
}
```

响应：

```protobuf
message GetGroupMembersResp {
  bool ok = 1;
  string message = 2;
  repeated GroupMemberData members = 3;
}
```

## 10. 客户端推荐实现

## 10.1 本地持久化

客户端应本地保存：

- 用户资料
- 联系人
- 群资料
- 消息列表
- 每个账号的 `last_inbox_seq`

不要依赖服务端长期保存消息历史。

## 10.2 登录后初始化流程

推荐顺序：

1. `AuthLogin`
2. 保存 `uid + token`
3. 建立 `ChatTunnel`
4. 发送 `LoginReq`
5. 调 `Sync(last_inbox_seq)`
6. 拉联系人、群列表
7. 进入主界面

## 10.3 断线重连流程

1. 重连 `ChatTunnel`
2. 重新发送 `LoginReq`
3. 调 `Sync(last_inbox_seq, limit)`
4. 处理缺失消息

## 10.4 消息去重

客户端至少要用下面两个字段做去重和顺序保护：

- `msg_id`
- `seq_id`

建议：

- 同一账号维度用 `msg_id` 去重
- 用 `seq_id` 做增量同步游标推进

## 10.5 发消息失败重试

如果发送超时或网络中断，可以复用原来的 `client_msg_id` 重试。

这样服务端会按幂等处理，不会重复写消息。

## 11. 错误处理建议

所有控制接口统一看：

- `ok`
- `message`

所有流式发送统一看：

- `SendResp.ok`
- `SendResp.reason`

客户端不要依赖固定错误码文本做复杂逻辑分支，优先按布尔状态和必要的字段判断。

## 12. 当前版本重要约束

- `GetVerifyCode` 未实现
- 一元接口当前主要基于 `uid` metadata 判断登录态
- 服务端不提供历史消息漫游
- 群聊当前只建议做小群
- 群消息对不同接收者的 `seq_id` 不同，这是正确行为

## 13. 推荐字段使用规范

### `client_msg_id`

要求：

- 每次新发消息必须生成新 UUID
- 重试同一条消息时必须复用同一个 UUID

### `trace_id`

可选。

推荐：

- 每次发送生成一个 trace id
- 日志排查时便于串联客户端和服务端链路

### `payload`

当前服务端只做透传，不关心业务内容。

客户端可以自行定义业务编码，例如：

- Protobuf
- JSON UTF-8
- FlatBuffers

但同一版本客户端必须保持一致。
