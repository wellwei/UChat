#include "MysqlMgr.h"


void TestMysql() {
    auto mysqlMgr = MysqlMgr::getInstance();

    std::string email = "1827104243@qq11.com";
    std::cout << mysqlMgr->getUidByEmail(email) << std::endl;

//    std::string password = "123456";
//    int uid = mysqlMgr->getUidByEmail(email);
//    if (mysqlMgr->resetPassword(uid, password)) {
//        std::cout << "Password reset successfully" << std::endl;
//    } else {
//        std::cout << "Failed to reset password" << std::endl;
//    }
}
