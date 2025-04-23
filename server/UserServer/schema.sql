-- UChat 用户服务的数据库架构

-- 如果数据库不存在则创建
CREATE DATABASE IF NOT EXISTS uchat;
USE uchat;

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    uid BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    password VARCHAR(256) NOT NULL,  -- 存储哈希后的密码
    nickname VARCHAR(64),
    avatar_url VARCHAR(256),
    email VARCHAR(128),
    phone VARCHAR(32),
    gender INT DEFAULT 0,  -- 0: 未知, 1: 男, 2: 女
    signature VARCHAR(256),
    location VARCHAR(256),
    status INT DEFAULT 0,  -- 0: 正常, 1: 禁用, 等等
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 好友关系表
CREATE TABLE IF NOT EXISTS friends (
    user_id BIGINT UNSIGNED NOT NULL,
    friend_id BIGINT UNSIGNED NOT NULL,
    status INT DEFAULT 0,  -- 0: 待处理, 1: 已接受, 2: 已拒绝, 等等
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, friend_id),
    FOREIGN KEY (user_id) REFERENCES users(uid) ON DELETE CASCADE,
    FOREIGN KEY (friend_id) REFERENCES users(uid) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 插入一些示例用户
INSERT INTO users (username, password, nickname, email, phone, avatar_url, signature, location, gender)
VALUES 
    ('user1', 'password1_hashed', '用户一', 'user1@example.com', '123-456-7890', 'https://example.com/avatars/user1.jpg', '你好世界！', '北京', 1),
    ('user2', 'password2_hashed', '用户二', 'user2@example.com', '123-456-7891', 'https://example.com/avatars/user2.jpg', '很高兴认识你！', '上海', 2);

-- 添加一个好友关系
INSERT INTO friends (user_id, friend_id, status)
VALUES (1, 2, 1);  -- 用户1和用户2是好友