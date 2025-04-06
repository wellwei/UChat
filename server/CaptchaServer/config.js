const fs = require('fs');

let config = JSON.parse(fs.readFileSync('config.json', 'utf8'));

let email_user = config.email.user;
let email_pass = config.email.pass;
let email_host = config.email.host;
let email_port = config.email.port;
let email_secure = config.email.secure;
let mysql_host = config.mysql.host;
let mysql_port = config.mysql.port;
let redis_host = config.redis.host;
let redis_port = config.redis.port;
let redis_password = config.redis.password;
let code_prefix = "code_";

module.exports = {
    email_user,
    email_pass,
    email_host,
    email_port,
    email_secure,
    mysql_host,
    mysql_port,
    redis_host,
    redis_port,
    redis_password,
}