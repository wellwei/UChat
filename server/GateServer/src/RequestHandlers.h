#ifndef GATESERVER_REQUESTHANDLERS_H
#define GATESERVER_REQUESTHANDLERS_H

#include "HttpConnection.h"
#include <memory>

class HttpConnection;

namespace RequestHandlerFuncs {

    void handle_get_test(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_getverifycode(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_register(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_resetPassword(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_login(const std::shared_ptr<HttpConnection> &connection);

    void handle_post_getChatServer(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_getUserProfile(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_getContacts(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_searchUser(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_sendContactRequest(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_handleContactRequest(const std::shared_ptr<HttpConnection> &connection);
    void handle_post_getContactRequests(const std::shared_ptr<HttpConnection> &connection);
}

#endif