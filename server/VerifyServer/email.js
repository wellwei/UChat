const nodemailer = require('nodemailer');
const config_module = require("./config");

/**
 * 邮件发送模块
 */
let transporter = nodemailer.createTransport({
    host: config_module.email_host,
    port: config_module.email_port,
    secure: config_module.email_secure,
    auth: {
        user: config_module.email_user,
        pass: config_module.email_pass,
    },
    tls: {
        rejectUnauthorized: false, // 忽略证书错误
    },
});

/**
 * 发送邮件
 * @param {*} mailOptions 邮件选项
 * @returns
 */
async function sendEmail(mailOptions) {
    return new Promise((resolve, reject) => {
        transporter.sendMail(mailOptions, (error, info) => {
            if (error) {
                console.error('Error sending email:', error);
                reject(error);
            } else {
                resolve(info.response);
            }
        });
    })
}

module.exports.sendEmail = sendEmail;