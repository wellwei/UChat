package service

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"mqgateway/internal/config"
	"mqgateway/internal/mq"
	pb "mqgateway/proto"
	"sync"
	"time"

	"github.com/streadway/amqp"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// MQGatewayService MQ网关服务实现
type MQGatewayService struct {
	pb.UnimplementedMQGatewayServiceServer
	mqManager   *mq.RabbitMQManager
	config      *config.Config
	subscribers map[string]chan *pb.MessageNotification
	subMutex    sync.RWMutex
}

// NewMQGatewayService 创建新的MQ网关服务
func NewMQGatewayService(cfg *config.Config, mqMgr *mq.RabbitMQManager) *MQGatewayService {
	return &MQGatewayService{
		mqManager:   mqMgr,
		config:      cfg,
		subscribers: make(map[string]chan *pb.MessageNotification),
	}
}

// PublishMessage 发布单条消息
func (s *MQGatewayService) PublishMessage(ctx context.Context, req *pb.PublishMessageRequest) (*pb.PublishMessageResponse, error) {
	log.Printf("收到发布消息请求: routing_key=%s, type=%s, target_user=%d",
		req.RoutingKey, req.MessageType.String(), req.TargetUserId)

	// 验证请求参数
	if req.RoutingKey == "" {
		return &pb.PublishMessageResponse{
			Success:      false,
			ErrorMessage: "路由键不能为空",
		}, nil
	}

	// 生成消息ID
	messageID := generateMessageID()

	// 解析payload
	var payload map[string]interface{}
	if req.Payload != "" {
		if err := json.Unmarshal([]byte(req.Payload), &payload); err != nil {
			return &pb.PublishMessageResponse{
				Success:      false,
				ErrorMessage: fmt.Sprintf("解析payload失败: %v", err),
			}, nil
		}
	}

	// 创建消息
	message := &mq.Message{
		ID:           messageID,
		RoutingKey:   req.RoutingKey,
		MessageType:  req.MessageType.String(),
		Payload:      payload,
		TargetUserID: req.TargetUserId,
		SenderUserID: req.SenderUserId,
		Timestamp:    req.Timestamp,
	}

	if message.Timestamp == 0 {
		message.Timestamp = time.Now().Unix()
	}

	// 发布消息到RabbitMQ
	if err := s.mqManager.PublishMessage(req.RoutingKey, message); err != nil {
		log.Printf("发布消息失败: %v", err)
		return &pb.PublishMessageResponse{
			Success:      false,
			ErrorMessage: fmt.Sprintf("发布消息失败: %v", err),
		}, nil
	}

	log.Printf("消息发布成功: message_id=%s", messageID)
	return &pb.PublishMessageResponse{
		Success:   true,
		MessageId: messageID,
	}, nil
}

// BatchPublish 批量发布消息
func (s *MQGatewayService) BatchPublish(ctx context.Context, req *pb.BatchPublishRequest) (*pb.BatchPublishResponse, error) {
	log.Printf("收到批量发布消息请求: 消息数量=%d", len(req.Messages))

	var messageIDs []string
	var errors []string

	for i, msg := range req.Messages {
		resp, err := s.PublishMessage(ctx, msg)
		if err != nil {
			errors = append(errors, fmt.Sprintf("消息%d发布失败: %v", i, err))
			continue
		}

		if !resp.Success {
			errors = append(errors, fmt.Sprintf("消息%d发布失败: %s", i, resp.ErrorMessage))
			continue
		}

		messageIDs = append(messageIDs, resp.MessageId)
	}

	success := len(errors) == 0
	var errorMessage string
	if !success {
		errorMessage = fmt.Sprintf("部分消息发布失败: %v", errors)
	}

	return &pb.BatchPublishResponse{
		Success:      success,
		ErrorMessage: errorMessage,
		MessageIds:   messageIDs,
	}, nil
}

// Subscribe 订阅消息
func (s *MQGatewayService) Subscribe(req *pb.SubscribeRequest, stream pb.MQGatewayService_SubscribeServer) error {
	log.Printf("收到订阅请求: queue=%s, routing_keys=%v", req.QueueName, req.RoutingKeys)

	if req.QueueName == "" {
		return status.Errorf(codes.InvalidArgument, "队列名称不能为空")
	}

	if err := s.mqManager.DeclareQueue(req.QueueName, req.RoutingKeys); err != nil {
		log.Printf("声明队列失败: %v", err)
		return status.Errorf(codes.Internal, "声明队列失败: %v", err)
	}

	deliveries, err := s.mqManager.ConsumeMessages(req.QueueName)
	if err != nil {
		log.Printf("开始消费消息失败: %v", err)
		return status.Errorf(codes.Internal, "开始消费消息失败: %v", err)
	}

	notificationChan := make(chan *pb.MessageNotification, 100)
	subscriberID := fmt.Sprintf("%s_%d", req.QueueName, time.Now().UnixNano())

	s.subMutex.Lock()
	s.subscribers[subscriberID] = notificationChan
	s.subMutex.Unlock()

	ctx, cancel := context.WithCancel(stream.Context())
	var wg sync.WaitGroup

	defer func() {
		s.subMutex.Lock()
		delete(s.subscribers, subscriberID)
		s.subMutex.Unlock()

		cancel()                // 1. 通知 handleMessages 停止
		wg.Wait()               // 2. 等待 handleMessages 完全退出
		close(notificationChan) // 3. 安全关闭 channel

		log.Printf("订阅者已断开并清理完毕: %s", subscriberID)
	}()

	wg.Add(1)
	go s.handleMessages(ctx, &wg, deliveries, notificationChan)

	for {
		select {
		case notification, ok := <-notificationChan:
			if !ok {
				log.Printf("通知通道已关闭: %s", subscriberID)
				return nil
			}
			if err := stream.Send(notification); err != nil {
				log.Printf("发送消息到客户端失败: %v", err)
				return err
			}
		case <-stream.Context().Done():
			log.Printf("客户端断开连接: %s", subscriberID)
			return nil
		}
	}
}

// HealthCheck 健康检查
func (s *MQGatewayService) HealthCheck(ctx context.Context, req *pb.HealthCheckRequest) (*pb.HealthCheckResponse, error) {
	healthy := s.mqManager.IsHealthy()
	status := "healthy"
	if !healthy {
		status = "unhealthy"
	}

	details := map[string]string{
		"rabbitmq":  status,
		"timestamp": time.Now().Format(time.RFC3339),
	}

	return &pb.HealthCheckResponse{
		Healthy: healthy,
		Status:  status,
		Details: details,
	}, nil
}

// handleMessages 处理RabbitMQ消息
func (s *MQGatewayService) handleMessages(ctx context.Context, wg *sync.WaitGroup, deliveries <-chan amqp.Delivery, notificationChan chan<- *pb.MessageNotification) {
	defer wg.Done()
	log.Printf("开始处理消息协程...")

	for {
		select {
		case delivery, ok := <-deliveries:
			if !ok {
				log.Printf("RabbitMQ deliveries channel 已关闭")
				return
			}

			// 解析消息
			var message mq.Message
			if err := json.Unmarshal(delivery.Body, &message); err != nil {
				log.Printf("解析消息失败: %v", err)
				delivery.Nack(false, false) // 拒绝消息，不重新入队
				continue
			}

			// 转换为protobuf消息类型
			messageType := pb.MessageType_CONTACT_REQUEST // 默认值
			switch message.MessageType {
			case "CONTACT_REQUEST":
				messageType = pb.MessageType_CONTACT_REQUEST
			case "CONTACT_ACCEPTED":
				messageType = pb.MessageType_CONTACT_ACCEPTED
			case "CONTACT_REJECTED":
				messageType = pb.MessageType_CONTACT_REJECTED
			case "CHAT_MESSAGE":
				messageType = pb.MessageType_CHAT_MESSAGE
			case "SYSTEM_NOTIFICATION":
				messageType = pb.MessageType_SYSTEM_NOTIFICATION
			}

			// 重新序列化payload
			payloadBytes, _ := json.Marshal(message.Payload)

			// 创建通知
			notification := &pb.MessageNotification{
				MessageId:    message.ID,
				RoutingKey:   message.RoutingKey,
				MessageType:  messageType,
				Payload:      string(payloadBytes),
				TargetUserId: message.TargetUserID,
				SenderUserId: message.SenderUserID,
				Timestamp:    message.Timestamp,
			}

			// 使用 select 防止在 notificationChan 关闭后写入
			select {
			case notificationChan <- notification:
				delivery.Ack(false)
			case <-ctx.Done():
				log.Printf("停止处理消息，因为上下文已取消。消息将不会被确认。")
				// 消息未被确认，RabbitMQ会重新投递
				return
			}
		case <-ctx.Done():
			log.Printf("停止处理消息协程，因为上下文已取消")
			return
		}
	}
}

// generateMessageID 生成唯一的消息ID
func generateMessageID() string {
	return fmt.Sprintf("msg_%d_%d", time.Now().UnixNano(), time.Now().Unix())
}
