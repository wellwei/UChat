const grpc = require('@grpc/grpc-js');
const message_proto = require('./proto');
const email_module = require("./email");
const config_module = require("./config");
const const_module = require("./const");

async function GetCaptcha(call, callback) {
    console.log("email is", call.request.email);
    try {
        // 生成验证码
        let code = Math.floor(Math.random() * 900000) + 100000; // 6位随机数
        let text_str = `您的验证码是：${code}，请在10分钟内使用。`;
        console.log("code is", code);
        // 发送邮件
        let mailOptions = {
            from: "UChat <" + config_module.email_user + ">",
            to: call.request.email,
            subject: '验证码',
            text: text_str,
        };
        let send_result = await email_module.sendEmail(mailOptions);
        console.log("send_result is", send_result);

        // 将验证码存入Redis TODO

        callback(null, { email: call.request.email, error: const_module.Errors.Success, captcha: code });
    }
    catch (error) {
        console.error("Error:", error);
        callback(null, { email: call.request.email, error: const_module.Errors.Exception });
    }
}

function main() {
    var server = new grpc.Server();
    server.addService(message_proto.CaptchaService.service, { GetCaptcha: GetCaptcha });
    server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
        console.log('grpc email server started');
    })
}

main();