#pragma once

// system includes
#include <memory>
#include <string_view>
#include <string>

// esp-idf includes
#include <asio.hpp>

class Webserver;
class ResponseHandler;

class ClientConnection : public std::enable_shared_from_this<ClientConnection>
{
public:
    ClientConnection(Webserver &webserver, asio::ip::tcp::socket socket);
    ~ClientConnection();

    Webserver &webserver() { return m_webserver; }
    const Webserver &webserver() const { return m_webserver; }

    asio::ip::tcp::socket &socket() { return m_socket; }
    const asio::ip::tcp::socket &socket() const { return m_socket; }

    const asio::ip::tcp::endpoint &remote_endpoint() const { return m_remote_endpoint; }

    void start();
    void responseFinished(std::error_code ec);

private:
    void doRead();
    void readyRead(std::error_code ec, std::size_t length);
    bool parseRequestLine(std::string_view line);
    bool readyReadLine(std::string_view line);
    bool parseRequestHeader(std::string_view line);

    Webserver &m_webserver;
    asio::ip::tcp::socket m_socket;
    const asio::ip::tcp::endpoint m_remote_endpoint;

    static constexpr const std::size_t max_length = 128;
    char m_receiveBuffer[max_length];

    std::string m_parsingBuffer;

    enum class State { RequestLine, RequestHeaders, RequestBody, Response };
    State m_state { State::RequestLine };

    std::size_t m_requestBodySize{};

    std::unique_ptr<ResponseHandler> m_responseHandler;
};
