package mq

import (
	"encoding/json"
	"fmt"
	"log"
	"mqgateway/internal/config"
	"sync"
	"time"

	"github.com/streadway/amqp"
)

// RabbitMQManager RabbitMQ管理器
type RabbitMQManager struct {
	config     *config.RabbitMQConfig
	connection *amqp.Connection
	channel    *amqp.Channel
	mutex      sync.RWMutex
	closed     bool
}

// Message 消息结构
type Message struct {
	ID           string                 `json:"id"`
	RoutingKey   string                 `json:"routing_key"`
	MessageType  string                 `json:"message_type"`
	Payload      map[string]interface{} `json:"payload"`
	TargetUserID uint64                 `json:"target_user_id"`
	SenderUserID uint64                 `json:"sender_user_id"`
	Timestamp    int64                  `json:"timestamp"`
}

// NewRabbitMQManager 创建新的RabbitMQ管理器
func NewRabbitMQManager(cfg *config.RabbitMQConfig) (*RabbitMQManager, error) {
	manager := &RabbitMQManager{
		config: cfg,
		closed: false,
	}

	if err := manager.connect(); err != nil {
		return nil, err
	}

	if err := manager.setupExchange(); err != nil {
		return nil, err
	}

	return manager, nil
}

// connect 连接到RabbitMQ
func (r *RabbitMQManager) connect() error {
	var err error
	r.connection, err = amqp.Dial(r.config.URL)
	if err != nil {
		return fmt.Errorf("连接RabbitMQ失败: %v", err)
	}

	r.channel, err = r.connection.Channel()
	if err != nil {
		return fmt.Errorf("创建RabbitMQ通道失败: %v", err)
	}

	// 监听连接关闭事件
	go r.handleConnectionClose()

	log.Printf("成功连接到RabbitMQ: %s", r.config.URL)
	return nil
}

// setupExchange 设置交换机
func (r *RabbitMQManager) setupExchange() error {
	return r.channel.ExchangeDeclare(
		r.config.Exchange,     // 交换机名称
		r.config.ExchangeType, // 交换机类型
		true,                  // 持久化
		false,                 // 自动删除
		false,                 // 内部使用
		false,                 // 不等待
		nil,                   // 参数
	)
}

// PublishMessage 发布消息
func (r *RabbitMQManager) PublishMessage(routingKey string, message *Message) error {
	r.mutex.RLock()
	defer r.mutex.RUnlock()

	if r.closed {
		return fmt.Errorf("RabbitMQ连接已关闭")
	}

	// 序列化消息
	body, err := json.Marshal(message)
	if err != nil {
		return fmt.Errorf("序列化消息失败: %v", err)
	}

	// 发布消息
	return r.channel.Publish(
		r.config.Exchange, // 交换机
		routingKey,        // 路由键
		false,             // 强制
		false,             // 立即
		amqp.Publishing{
			ContentType:  "application/json",
			Body:         body,
			DeliveryMode: amqp.Persistent, // 持久化消息
			Timestamp:    time.Now(),
			MessageId:    message.ID,
		},
	)
}

// DeclareQueue 声明队列
func (r *RabbitMQManager) DeclareQueue(queueName string, routingKeys []string) error {
	r.mutex.RLock()
	defer r.mutex.RUnlock()

	if r.closed {
		return fmt.Errorf("RabbitMQ连接已关闭")
	}

	// 声明队列
	_, err := r.channel.QueueDeclare(
		queueName, // 队列名称
		true,      // 持久化
		false,     // 自动删除
		false,     // 独占
		false,     // 不等待
		nil,       // 参数
	)
	if err != nil {
		return fmt.Errorf("声明队列失败: %v", err)
	}

	// 绑定路由键
	for _, routingKey := range routingKeys {
		err = r.channel.QueueBind(
			queueName,         // 队列名称
			routingKey,        // 路由键
			r.config.Exchange, // 交换机
			false,             // 不等待
			nil,               // 参数
		)
		if err != nil {
			return fmt.Errorf("绑定队列失败: %v", err)
		}
	}

	log.Printf("成功声明队列: %s, 路由键: %v", queueName, routingKeys)
	return nil
}

// ConsumeMessages 消费消息
func (r *RabbitMQManager) ConsumeMessages(queueName string) (<-chan amqp.Delivery, error) {
	r.mutex.RLock()
	defer r.mutex.RUnlock()

	if r.closed {
		return nil, fmt.Errorf("RabbitMQ连接已关闭")
	}

	return r.channel.Consume(
		queueName, // 队列名称
		"",        // 消费者标签
		false,     // 自动确认
		false,     // 独占
		false,     // 不等待本地
		false,     // 不等待
		nil,       // 参数
	)
}

// Close 关闭连接
func (r *RabbitMQManager) Close() error {
	r.mutex.Lock()
	defer r.mutex.Unlock()

	if r.closed {
		return nil
	}

	r.closed = true

	if r.channel != nil {
		r.channel.Close()
	}

	if r.connection != nil {
		r.connection.Close()
	}

	log.Println("RabbitMQ连接已关闭")
	return nil
}

// IsHealthy 检查健康状态
func (r *RabbitMQManager) IsHealthy() bool {
	r.mutex.RLock()
	defer r.mutex.RUnlock()

	return !r.closed && r.connection != nil && !r.connection.IsClosed()
}

// handleConnectionClose 处理连接关闭事件
func (r *RabbitMQManager) handleConnectionClose() {
	closeError := <-r.connection.NotifyClose(make(chan *amqp.Error))
	if closeError != nil {
		log.Printf("RabbitMQ连接关闭: %v", closeError)
		r.mutex.Lock()
		r.closed = true
		r.mutex.Unlock()

		// 这里可以添加重连逻辑
		go r.reconnect()
	}
}

// reconnect 重连逻辑
func (r *RabbitMQManager) reconnect() {
	for {
		time.Sleep(5 * time.Second)
		log.Println("尝试重连RabbitMQ...")

		r.mutex.Lock()
		if err := r.connect(); err != nil {
			log.Printf("重连失败: %v", err)
			r.mutex.Unlock()
			continue
		}

		if err := r.setupExchange(); err != nil {
			log.Printf("设置交换机失败: %v", err)
			r.mutex.Unlock()
			continue
		}

		r.closed = false
		r.mutex.Unlock()

		log.Println("RabbitMQ重连成功")
		break
	}
}
