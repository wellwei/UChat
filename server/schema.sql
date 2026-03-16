CREATE TABLE IF NOT EXISTS users (                                                                                                                           
     uid BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,                                                                                                          
     username VARCHAR(50) NOT NULL UNIQUE,                                                                                                                    
     password VARCHAR(255) NOT NULL,                                                                                                                          
     email VARCHAR(100) NOT NULL UNIQUE,                                                                                                                      
    nickname VARCHAR(100) DEFAULT '',                                                                                                                        
     avatar_url VARCHAR(255) DEFAULT '',                                                                                                                      
     phone VARCHAR(20) DEFAULT '',                                                                                                                            
     gender TINYINT DEFAULT 0 COMMENT '0=OTHER, 1=MALE, 2=FEMALE',                                                                                            
     signature VARCHAR(255) DEFAULT '',                                                                                                                       
     location VARCHAR(100) DEFAULT '',                                                                                                                        
     status TINYINT DEFAULT 0 COMMENT '0=OFFLINE, 1=ONLINE, 2=BUSY, etc.',                                                                                    
     create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,                                                                                                         
     update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,                                                                             
     INDEX idx_username (username),                                                                                                                           
     INDEX idx_email (email)                                                                                                                                  
 ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS contact_requests (
    request_id  BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    from_uid    BIGINT UNSIGNED NOT NULL,
    to_uid      BIGINT UNSIGNED NOT NULL,
    note        VARCHAR(255) DEFAULT '',
    status      TINYINT DEFAULT 0 COMMENT '0=PENDING, 1=ACCEPTED, 2=REJECTED',
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_from_to (from_uid, to_uid),
    INDEX idx_to_uid (to_uid),
    INDEX idx_from_uid (from_uid),
    FOREIGN KEY (from_uid) REFERENCES users(uid),
    FOREIGN KEY (to_uid)   REFERENCES users(uid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS contacts (
    id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    owner_uid   BIGINT UNSIGNED NOT NULL,
    contact_uid BIGINT UNSIGNED NOT NULL,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_owner_contact (owner_uid, contact_uid),
    INDEX idx_owner_uid (owner_uid),
    FOREIGN KEY (owner_uid)   REFERENCES users(uid),
    FOREIGN KEY (contact_uid) REFERENCES users(uid)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
