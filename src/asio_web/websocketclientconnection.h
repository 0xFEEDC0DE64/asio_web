#pragma once

// system includes

// esp-idf includes
#include <asio.hpp>

class Webserver;
class ResponseHandler;

class WebsocketClientConnection : public std::enable_shared_from_this<WebsocketClientConnection>
{
public:
    WebsocketClientConnection(Webserver &webserver, asio::ip::tcp::socket socket, std::string &&parsingBuffer, std::unique_ptr<ResponseHandler> &&responseHandler);
    ~WebsocketClientConnection();

    Webserver &webserver() { return m_webserver; }
    const Webserver &webserver() const { return m_webserver; }

    asio::ip::tcp::socket &socket() { return m_socket; }
    const asio::ip::tcp::socket &socket() const { return m_socket; }

    const asio::ip::tcp::endpoint &remote_endpoint() const { return m_remote_endpoint; }

    void start();

private:
    void doReadWebSocket();
    void readyReadWebSocket(std::error_code ec, std::size_t length);

    void sendMessage(bool fin, uint8_t reserved, uint8_t opcode, bool mask, std::string_view payload);
    void onMessageSent(std::error_code ec, std::size_t length);

    Webserver &m_webserver;
    asio::ip::tcp::socket m_socket;
    const asio::ip::tcp::endpoint m_remote_endpoint;

    static constexpr const std::size_t max_length = 1024;
    char m_receiveBuffer[max_length];

    std::string m_parsingBuffer;

    std::unique_ptr<ResponseHandler> m_responseHandler;

    std::string m_sendBuffer;
};
