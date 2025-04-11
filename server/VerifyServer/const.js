let code_prefix = 'verify_code_';

const Errors = {
    Success: 0,
    RedisError: 1,
    EmailError: 2,
    Exception: 3,
    InvalidArgument: 4,
}

module.exports = { code_prefix, Errors };