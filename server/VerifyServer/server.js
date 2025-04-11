const grpc = require('@grpc/grpc-js');
const message_proto = require('./proto');
const email_module = require("./email");
const config_module = require("./config");
const const_module = require("./const");
const redis_module = require("./redis");

async function GetVerifyCode(call, callback) {
    console.log("Email: ", call.request.email);
    try {
        const email = call.request.email;
        const verifyCode = Math.floor(Math.random() * 900000) + 100000;
        const redis_key = const_module.code_prefix + email;

        let result = await redis_module.set(redis_key, verifyCode, 60 * 10);    // 10分钟有效期
        if (!result) {
            console.error("Failed to set verify code to redis");
            callback(null, { email: email, error: const_module.Errors.RedisError });
            return;
        }

        const mailOptions = {
            from: config_module.email_user,
            to: email,
            subject: "UChat 验证码",
            text: `您的验证码是：${verifyCode}，请在10分钟内使用。`,
            html: `<p>您的验证码是：<strong>${verifyCode}</strong>，请在10分钟内使用。</p>`
        }

        let send_result = await email_module.sendEmail(mailOptions);
        if (!send_result) {
            console.error("Failed to send email");
            callback(null, { email: email, error: const_module.Errors.EmailError });
            return;
        }

        callback(null, { email: email, error: const_module.Errors.Success });
    }
    catch (error) {
        console.error("Error:", error);
        callback(null, { email: call.request.email, error: const_module.Errors.Exception });
    }
}

function main() {
    var server = new grpc.Server();
    server.addService(message_proto.VerifyService.service, { GetVerifyCode: GetVerifyCode });
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        console.log('grpc email server started');
    })
}

main();