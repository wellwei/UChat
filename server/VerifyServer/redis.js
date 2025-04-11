const redis = require('ioredis');
const config_module = require("./config");

const redis_client = new redis({
    host: config_module.redis_host,
    port: config_module.redis_port,
    password: config_module.redis_password,
});

/**
 * 监听Redis错误
 */
redis_client.on("error", (err) => {
    console.error("Redis Error: ", err);
    redis_client.quit();
})

/**
 * 根据 key 获取 value
 */
async function get(key) {
    try {
        let value = await redis_client.get(key);
        if (value === null) {
            console.error("Redis Get Error: ", "Key not found");
            return null;
        }
        return value;
    } catch (err) {
        console.error("Redis Get Error: ", err);
        return null;
    }
}

/**
 * 设置 key 的 value 和过期时间
 * @param {string} key 键
 * @param {string} value 值
 * @param {number} ttl 过期时间
 */
async function set(key, value, ttl = -1) {
    try {
        if (ttl <= 0) {
            await redis_client.set(key, value);
        } else {
            await redis_client.setex(key, ttl, value);
        }
        return true;
    } catch (err) {
        console.error("Redis Set Error: ", err);
        return false;
    }
}

/**
 * 查找 key 是否存在
 */
async function exists(key) {
    try {
        let result = await redis_client.exists(key);
        return result === 1;
    } catch (err) {
        console.error("Redis Exists Error: ", err);
        return false;
    }
}

/**
 * 关闭 Redis 连接
 */
async function quit() {
    await redis_client.quit();
}

module.exports = {
    get,
    set,
    exists,
    quit,
}