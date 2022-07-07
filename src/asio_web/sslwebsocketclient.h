#pragma once

// system include
#include <queue>
#include <optional>

// esp-idf includes
#include <asio.hpp>
#include <asio/ssl.hpp>

class SslWebsocketClient
{
public:
    SslWebsocketClient(asio::io_context &io_context, std::string &&host, std::string &&port, std::string &&path);
    SslWebsocketClient(asio::io_context &io_context, const std::string &host, const std::string &port, const std::string &path);

    void start();

    virtual void handleConnected() = 0;
    virtual void handleMessage(bool fin, uint8_t reserved, uint8_t opcode, bool mask, std::string_view payload) = 0;

private:
    void resolve();
    void onResolved(const std::error_code &error, asio::ip::tcp::resolver::iterator iterator);
    void connect(const asio::ip::tcp::resolver::iterator &endpoints);
    void onConnected(const std::error_code &error);
    void handshake();
    void onHandshaked(const std::error_code & error);
    void send_request();
    void onSentRequest(const std::error_code &error, std::size_t length);
    void receive_response();
    void onReceivedResponse(const std::error_code &error, std::size_t length);
    bool readyReadLine(std::string_view line);
    bool parseResponseLine(std::string_view line);
    bool parseResponseHeader(std::string_view line);
    void doReadWebSocket();
    void onReceiveWebsocket(const std::error_code &error, std::size_t length);

public:
    void sendMessage(bool fin, uint8_t reserved, uint8_t opcode, bool mask, std::string_view payload);

private:
    void onMessageSent(std::error_code ec, std::size_t length);

    std::string m_host;
    std::string m_port;
    std::string m_path;

    asio::ip::tcp::resolver m_resolver;
    asio::ssl::context m_sslCtx{asio::ssl::context::tls_client};
    asio::ssl::stream<asio::ip::tcp::socket> m_socket;
    //asio::ip::tcp::socket m_socket;
    char m_receiveBuffer[1024];

    enum class State { Request, ResponseLine, ResponseHeaders, ResponseBody, WebSocket };
    State m_state { State::Request };

    std::string m_parsingBuffer;

    std::size_t m_responseBodySize{};

    std::optional<std::string> m_sending;
    std::queue<std::string> m_sendingQueue;
};
