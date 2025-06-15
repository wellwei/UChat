package main

import (
	"flag"
	"log"
	"mqgateway/internal/config"
	"mqgateway/internal/mq"
	"mqgateway/internal/service"
	pb "mqgateway/proto"
	"net"
	"os"
	"os/signal"
	"syscall"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

func main() {
	// 解析命令行参数
	configPath := flag.String("config", "config.yaml", "配置文件路径")
	flag.Parse()

	// 加载配置
	cfg, err := config.LoadConfig(*configPath)
	if err != nil {
		log.Fatalf("加载配置失败: %v", err)
	}

	log.Printf("MQGateway 启动中...")
	log.Printf("配置文件: %s", *configPath)
	log.Printf("服务地址: %s", cfg.GetServerAddress())
	log.Printf("RabbitMQ URL: %s", cfg.RabbitMQ.URL)

	// 创建RabbitMQ管理器
	mqManager, err := mq.NewRabbitMQManager(&cfg.RabbitMQ)
	if err != nil {
		log.Fatalf("创建RabbitMQ管理器失败: %v", err)
	}
	defer func(mqManager *mq.RabbitMQManager) {
		err := mqManager.Close()
		if err != nil {
			log.Fatalf("<UNK>MQ<UNK>: %v", err)
		}
	}(mqManager)

	// 创建gRPC服务器
	grpcServer := grpc.NewServer(
		grpc.MaxRecvMsgSize(10*1024*1024), // 10MB
		grpc.MaxSendMsgSize(10*1024*1024), // 10MB
	)

	// 注册服务
	mqGatewayService := service.NewMQGatewayService(cfg, mqManager)
	pb.RegisterMQGatewayServiceServer(grpcServer, mqGatewayService)

	// 启用反射（用于调试）
	reflection.Register(grpcServer)

	// 监听端口
	listener, err := net.Listen("tcp", cfg.GetServerAddress())
	if err != nil {
		log.Fatalf("监听端口失败: %v", err)
	}

	// 启动服务器
	go func() {
		log.Printf("MQGateway 服务启动成功，监听地址: %s", cfg.GetServerAddress())
		if err := grpcServer.Serve(listener); err != nil {
			log.Fatalf("启动gRPC服务器失败: %v", err)
		}
	}()

	// 等待中断信号
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// 阻塞等待信号
	sig := <-sigChan
	log.Printf("收到信号: %v，开始优雅关闭...", sig)

	// 优雅关闭
	grpcServer.GracefulStop()
	log.Println("MQGateway 服务已关闭")
}
