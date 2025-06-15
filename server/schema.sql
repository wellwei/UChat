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

-- 联系人关系表
CREATE TABLE IF NOT EXISTS contacts (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id_a BIGINT UNSIGNED NOT NULL,
    user_id_b BIGINT UNSIGNED NOT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uk_contact_pair (user_id_a, user_id_b),
    INDEX idx_user_id_b (user_id_b),
    FOREIGN KEY (user_id_a) REFERENCES users(uid) ON DELETE CASCADE,
    FOREIGN KEY (user_id_b) REFERENCES users(uid) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 联系人请求表
CREATE TABLE IF NOT EXISTS contact_requests (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    requester_id BIGINT UNSIGNED NOT NULL COMMENT '请求发起者ID',
    addressee_id BIGINT UNSIGNED NOT NULL COMMENT '请求接收者ID',
    status TINYINT NOT NULL DEFAULT 0 COMMENT '0: PENDING, 1: ACCEPTED, 2: REJECTED',
    request_message VARCHAR(255) DEFAULT '' COMMENT '验证信息',
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    INDEX idx_addressee_status (addressee_id, status),
    FOREIGN KEY (requester_id) REFERENCES users(uid) ON DELETE CASCADE,
    FOREIGN KEY (addressee_id) REFERENCES users(uid) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 插入一些示例用户
INSERT INTO users (username, password, nickname, email, phone, avatar_url, signature, location, gender)
VALUES 
    ('user1', 'password1_hashed', '用户一', 'user1@example.com', '123-456-7890', 'https://example.com/avatars/user1.jpg', '你好世界！', '北京', 1),
    ('user2', 'password2_hashed', '用户二', 'user2@example.com', '123-456-7891', 'https://example.com/avatars/user2.jpg', '很高兴认识你！', '上海', 2);

-- 添加一个好友关系
INSERT INTO contacts (user_id_a, user_id_b)
VALUES (1, 2);  -- 用户1和用户2是好友