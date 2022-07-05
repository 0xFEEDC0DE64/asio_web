#pragma once

#include <asio.hpp>
#include <asio/ssl.hpp>

class WebsocketClient
{
public:
    WebsocketClient(asio::io_context &io_context);

    void start();

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

    void sendMessage(bool fin, uint8_t reserved, uint8_t opcode, bool mask, std::string_view payload);
    void onMessageSent(std::error_code ec, std::size_t length, std::size_t expectedLength);

    asio::ip::tcp::resolver m_resolver;
    //asio::ssl::context m_sslCtx{asio::ssl::context::tls_client};
    //asio::ssl::stream<asio::ip::tcp::socket> m_socket;
    asio::ip::tcp::socket m_socket;
    char m_receiveBuffer[1024];

    enum class State { Request, ResponseLine, ResponseHeaders, ResponseBody, WebSocket };
    State m_state { State::Request };

    std::string m_parsingBuffer;

    std::size_t m_responseBodySize{};
};
