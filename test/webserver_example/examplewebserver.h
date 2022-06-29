#pragma once

#include "asio_webserver/webserver.h"

class ExampleWebserver : public Webserver
{
public:
    using Webserver::Webserver;

    std::unique_ptr<ResponseHandler> makeResponseHandler(ClientConnection &clientConnection, std::string_view method, std::string_view path, std::string_view protocol) final;
};
