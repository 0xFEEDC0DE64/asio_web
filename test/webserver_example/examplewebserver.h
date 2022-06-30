#pragma once

// 3rdparty lib includes
#include <asio_webserver/webserver.h>

class ExampleWebserver final : public Webserver
{
public:
    using Webserver::Webserver;

    bool connectionKeepAlive() const final { return true; }

    std::unique_ptr<ResponseHandler> makeResponseHandler(ClientConnection &clientConnection, std::string_view method, std::string_view path, std::string_view protocol) final;
};
