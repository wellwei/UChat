#include <mysql/jdbc.h>
#include "ConfigMgr.h"

// 在连接前后添加内存检查
void TestMysql() {

    std::string host = "3486vskn5236.vicp.fun";
    std::string port = "18057";
    std::string user = "root";
    std::string password = "dPGNaHRTj4b2crnt";

    sql::mysql::MySQL_Driver *driver = nullptr;
    try {
        driver = sql::mysql::get_mysql_driver_instance();
        std::unique_ptr<sql::Connection> con(driver->connect("tcp://" + host + ":" + port, user, password));
        std::cout << "Connected to MySQL server at " << host << ":" << port << " as user " << user << std::endl;

        con->close();
        // 移除 driver->threadEnd(); 除非你明确知道需要它
    } catch (...) {
        std::cerr << "Failed to connect to MySQL server at " << host << ":" << port << " as user " << user << std::endl;
        exit(1);
    }

    if (driver) {
        driver->threadEnd();
    }
}
