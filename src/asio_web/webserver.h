#pragma once

// system includes
#include <memory>
#include <string_view>
#include <atomic>

// esp-idf includes
#include <asio.hpp>

// forward declares
class ResponseHandler;
class ClientConnection;

class Webserver
{
public:
    Webserver(asio::io_context& io_context, unsigned short port);
    virtual ~Webserver() = default;

    virtual bool connectionKeepAlive() const = 0;

    virtual std::unique_ptr<ResponseHandler> makeResponseHandler(ClientConnection &clientConnection, std::string_view method, std::string_view path, std::string_view protocol) = 0;

protected:
    friend class ClientConnection;
    friend class WebsocketClientConnection;
    std::atomic<int> m_httpClients;
    std::atomic<int> m_websocketClients;

private:
    void doAccept();
    void acceptClient(std::error_code ec, asio::ip::tcp::socket socket);

    asio::ip::tcp::acceptor m_acceptor;
};
