#pragma once

#include <memory>
#include <string_view>

#include <asio.hpp>

class ResponseHandler;
class ClientConnection;

class Webserver
{
public:
    Webserver(asio::io_context& io_context, short port);
    virtual ~Webserver() = default;

    virtual std::unique_ptr<ResponseHandler> makeResponseHandler(ClientConnection &clientConnection, std::string_view method, std::string_view path, std::string_view protocol) = 0;

private:
    void doAccept();
    void acceptClient(std::error_code ec, asio::ip::tcp::socket socket);

    asio::ip::tcp::acceptor m_acceptor;
};
