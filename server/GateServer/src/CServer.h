//
// Created by wellwei on 2025/4/1.
//

#ifndef GATESERVER_CSERVER_H
#define GATESERVER_CSERVER_H

#include "global.h"

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace asio = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;

class CServer : public std::enable_shared_from_this<CServer> {
public:
    CServer(asio::io_context& ioc, unsigned short &port);
    ~CServer() = default;

    void start();

private:
    asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};


#endif //GATESERVER_CSERVER_H
