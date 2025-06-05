#include <iostream>
#include "StatusServer.h"
#include "ConfigMgr.h"
#include "Logger.h"

int main() {
    Logger::init();

    ConfigMgr::getInstance();

    StatusServer status_server;
    status_server.start();

    return 0;
}
