//
// Created by wellwei on 2025/4/9.
//

#include "RedisMgr.h"
#include "RedisConnPool.h"

void TestRedisMgr() {
    auto redis_mgr = RedisMgr::getInstance();

    // 测试设置和获取 Redis String 键值
    redis_mgr->set("key1", "value1");
    auto get_value = redis_mgr->get("key1");
    if (get_value) {
        std::cout << "Get value: " << *get_value << std::endl;
    } else {
        std::cout << "Failed to get value" << std::endl;
    }

    // 测试删除 Redis String 键值
    redis_mgr->del("key1");
    auto check_value = redis_mgr->get("key1");
    if (check_value) {
        std::cout << "Value still exists: " << *check_value << std::endl;
    } else {
        std::cout << "Value deleted successfully" << std::endl;
    }

    // 测试设置和获取 Redis Hash 键值
    redis_mgr->hset("key2", "field2", "field_value2");
    auto get_field_value = redis_mgr->hget("key2", "field2");
    if (get_field_value) {
        std::cout << "Get field value: " << *get_field_value << std::endl;
    } else {
        std::cout << "Failed to get field value" << std::endl;
    }

    // 测试删除 Redis Hash 键值
    redis_mgr->hdel("key2", "field2");
    auto check_field_value = redis_mgr->hget("key2", "field2");
    if (check_field_value) {
        std::cout << "Field value still exists: " << *check_field_value << std::endl;
    } else {
        std::cout << "Field value deleted successfully" << std::endl;
    }

    // 测试设置 Redis String 键值的过期时间
    redis_mgr->set("key3", "value3", 5); // 设置过期时间为 5 秒
    std::this_thread::sleep_for(std::chrono::seconds(6)); // 等待 6 秒
    auto expired_value = redis_mgr->get("key3");
    if (expired_value) {
        std::cout << "Value still exists after expiration: " << *expired_value << std::endl;
    } else {
        std::cout << "Value expired successfully" << std::endl;
    }

    // 测试 Redis List 操作
    redis_mgr->lpush("key4", "value1");
    redis_mgr->lpush("key4", "value2");
    redis_mgr->lpush("key4", "value3");
    auto lpop_value = redis_mgr->lpop("key4");
    if (lpop_value) {
        std::cout << "LPOP value: " << *lpop_value << std::endl;
    } else {
        std::cout << "Failed to LPOP value" << std::endl;
    }

    auto rpop_value = redis_mgr->rpop("key4");
    if (rpop_value) {
        std::cout << "RPOP value: " << *rpop_value << std::endl;
    } else {
        std::cout << "Failed to RPOP value" << std::endl;
    }


    // 测试 Redis List 删除
    redis_mgr->lpush("key4", "value6");
    redis_mgr->lpush("key4", "value7");
    redis_mgr->lpush("key4", "value8");
    redis_mgr->del("key4");
    auto list_deleted_value = redis_mgr->lpop("key4");
    if (list_deleted_value) {
        std::cout << "List value still exists after deletion: " << *list_deleted_value << std::endl;
    } else {
        std::cout << "List deleted successfully" << std::endl;
    }

    // 测试 Redis List 过期时间
    redis_mgr->lpush("key5", "value4");
    redis_mgr->lpush("key5", "value5");
    redis_mgr->expire("key5", 5); // 设置过期时间为 5 秒
    std::this_thread::sleep_for(std::chrono::seconds(6)); // 等待 6 秒
    auto list_value = redis_mgr->lpop("key5");
    if (list_value) {
        std::cout << "List value still exists after expiration: " << *list_value << std::endl;
    } else {
        std::cout << "List value expired successfully" << std::endl;
    }

    // 测试 Redis Hash 过期时间
    redis_mgr->hset("key6", "field6", "field_value6");
    redis_mgr->expire("key6", 5); // 设置过期时间为 5 秒
    std::this_thread::sleep_for(std::chrono::seconds(6)); // 等待 6 秒
    auto hash_value = redis_mgr->hget("key6", "field6");
    if (hash_value) {
        std::cout << "Hash value still exists after expiration: " << *hash_value << std::endl;
    } else {
        std::cout << "Hash value expired successfully" << std::endl;
    }

    // 测试 Redis Hash 存在性
    redis_mgr->hset("key7", "field7", "field_value7");
    bool exists = redis_mgr->hexists("key7", "field7");
    if (exists) {
        std::cout << "Hash field exists" << std::endl;
    } else {
        std::cout << "Hash field does not exist" << std::endl;
    }

    // 测试 Redis Hash 删除
    redis_mgr->hdel("key7", "field7");
    bool hash_deleted = redis_mgr->hexists("key7", "field7");
    if (hash_deleted) {
        std::cout << "Hash field still exists after deletion: " << *check_field_value << std::endl;
    } else {
        std::cout << "Hash field deleted successfully" << std::endl;
    }

    // 测试 Redis String 存在性
    redis_mgr->set("key8", "value8");
    bool string_exists = redis_mgr->exists("key8");
    if (string_exists) {
        std::cout << "String key exists" << std::endl;
    } else {
        std::cout << "String key does not exist" << std::endl;
    }

    // 测试 Redis String 删除
    redis_mgr->del("key8");
    bool string_deleted = redis_mgr->exists("key8");
    if (string_deleted) {
        std::cout << "String key still exists after deletion: " << *check_value << std::endl;
    } else {
        std::cout << "String key deleted successfully" << std::endl;
    }
}