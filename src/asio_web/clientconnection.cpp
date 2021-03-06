#include "clientconnection.h"

// system includes
#include <cstdio>
#include <utility>

// esp-idf includes
#include <esp_log.h>

// 3rdparty lib includes
#include <numberparsing.h>
#include <strutils.h>

// local includes
#include "webserver.h"
#include "responsehandler.h"
#include "websocketclientconnection.h"

namespace {
constexpr const char * const TAG = "ASIO_WEB";
} // namespace

ClientConnection::ClientConnection(Webserver &webserver, asio::ip::tcp::socket socket) :
    m_webserver{webserver},
    m_socket{std::move(socket)},
    m_remote_endpoint{m_socket.remote_endpoint()}
{
    ESP_LOGI(TAG, "new client (%s:%hi)",
             m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());

    m_webserver.m_httpClients++;
}

ClientConnection::~ClientConnection()
{
    ESP_LOGI(TAG, "client destroyed (%s:%hi)",
             m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());

    m_webserver.m_httpClients--;
}

void ClientConnection::start()
{
    doRead();
}

void ClientConnection::responseFinished(std::error_code ec)
{
    if (ec)
    {
        ESP_LOGW(TAG, "error: %i (%s:%hi)", ec.value(),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        m_socket.close();
        return;
    }

    if (m_webserver.connectionKeepAlive())
    {
//        ESP_LOGD(TAG, "state changed to RequestLine");
        m_state = State::RequestLine;

        doRead();
    }
    else
        m_socket.close();
}

void ClientConnection::upgradeWebsocket()
{
//    ESP_LOGD(TAG, "state changed to RequestLine");
    m_state = State::WebSocket;

    std::make_shared<WebsocketClientConnection>(m_webserver, std::move(m_socket), std::move(m_parsingBuffer), std::move(m_responseHandler))->start();
}

void ClientConnection::doRead()
{
    m_socket.async_read_some(asio::buffer(m_receiveBuffer, max_length),
                             [this, self=shared_from_this()](std::error_code ec, std::size_t length)
                             { readyRead(ec, length); });
}

void ClientConnection::readyRead(std::error_code ec, std::size_t length)
{
    if (ec)
    {
        ESP_LOGI(TAG, "error: %i (%s:%hi)", ec.value(),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        return;
    }

    if (m_state == State::RequestBody)
    {
        if (!m_responseHandler)
        {
            ESP_LOGW(TAG, "invalid response handler (%s:%hi)",
                     m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
            m_socket.close();
            return;
        }

        if (!m_requestBodySize)
            goto requestFinished;
        else
        {
            if (length <= m_requestBodySize)
            {
                m_responseHandler->requestBodyReceived({m_receiveBuffer, length});
                m_requestBodySize -= length;
                length = 0;

                if (!m_requestBodySize)
                    goto requestFinished;
            }
            else
            {
                m_responseHandler->requestBodyReceived({m_receiveBuffer, m_requestBodySize});
                // TODO how to erase from m_receiveBuffer ?
                m_requestBodySize = 0;
                goto requestFinished;
            }

requestFinished:
//            ESP_LOGV(TAG, "state changed to Response");
            m_state = State::Response;

            m_responseHandler->sendResponse();
        }
    }

//    ESP_LOGV(TAG, "received: %zd \"%.*s\"", length, length, m_receiveBuffer);
    m_parsingBuffer.append(m_receiveBuffer, length);

    bool shouldDoRead{true};

    while (true)
    {
        constexpr std::string_view newLine{"\r\n"};
        const auto index = m_parsingBuffer.find(newLine.data(), 0, newLine.size());
        if (index == std::string::npos)
            break;

        std::string line{m_parsingBuffer.data(), index};

//        ESP_LOGD(TAG, "line: %zd \"%.*s\"", line.size(), line.size(), line.data());

        m_parsingBuffer.erase(std::begin(m_parsingBuffer), std::next(std::begin(m_parsingBuffer), line.size() + newLine.size()));

        if (!readyReadLine(line))
            shouldDoRead = false;
    }

    if (shouldDoRead)
        doRead();
}

bool ClientConnection::readyReadLine(std::string_view line)
{
    switch (m_state)
    {
    case State::RequestLine:
//        ESP_LOGV(TAG, "case State::RequestLine:");
        return parseRequestLine(line);
    case State::RequestHeaders:
//        ESP_LOGV(TAG, "case State::RequestHeaders:");
        return parseRequestHeader(line);
    case State::RequestBody:
//        ESP_LOGV(TAG, "case State::RequestBody:");
        ESP_LOGW(TAG, "unexpected state=RequestBody (%s:%hi)",
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        return true;
    case State::Response:
//        ESP_LOGV(TAG, "case State::Response:");
        ESP_LOGW(TAG, "unexpected state=Response (%s:%hi)",
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        return true;
    default:
        ESP_LOGW(TAG, "unknown state %i (%s:%hi)", std::to_underlying(m_state),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        return true;
    }
}

bool ClientConnection::parseRequestLine(std::string_view line)
{
//    ESP_LOGV(TAG, "%.*s", line.size(), line.data());

    if (const auto index = line.find(' '); index == std::string::npos)
    {
        ESP_LOGW(TAG, "invalid request line (1): \"%.*s\" (%s:%hi)", line.size(), line.data(),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        m_socket.close();
        return false;
    }
    else
    {
        const std::string_view method { line.data(), index };
//        ESP_LOGV(TAG, "request method: %zd \"%.*s\"", method.size(), method.size(), method.data());

        if (const auto index2 = line.find(' ', index + 1); index2 == std::string::npos)
        {
            ESP_LOGW(TAG, "invalid request line (2): \"%.*s\" (%s:%hi)", line.size(), line.data(),
                     m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
            m_socket.close();
            return false;
        }
        else
        {
            const std::string_view path { line.data() + index + 1, line.data() + index2 };
//            ESP_LOGV(TAG, "request path: %zd \"%.*s\"", path.size(), path.size(), path.data());

            const std::string_view protocol { line.cbegin() + index2 + 1, line.cend() };
//            ESP_LOGV(TAG, "request protocol: %zd \"%.*s\"", protocol.size(), protocol.size(), protocol.data());

            m_responseHandler = m_webserver.makeResponseHandler(*this, method, path, protocol);
            if (!m_responseHandler)
            {
                ESP_LOGW(TAG, "invalid response handler method=\"%.*s\" path=\"%.*s\" protocol=\"%.*s\" (%s:%hi)",
                         method.size(), method.data(), path.size(), path.data(), protocol.size(), protocol.data(),
                         m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
                m_socket.close();
                return false;
            }

//            ESP_LOGV(TAG, "state changed to RequestHeaders");
            m_state = State::RequestHeaders;

            return true;
        }
    }
}

bool ClientConnection::parseRequestHeader(std::string_view line)
{
//    ESP_LOGV(TAG, "%.*s", line.size(), line.data());

    if (!line.empty())
    {
        constexpr std::string_view sep{": "};
        if (const auto index = line.find(sep.data(), 0, sep.size()); index == std::string_view::npos)
        {
            ESP_LOGW(TAG, "invalid request header: %zd \"%.*s\" (%s:%hi)", line.size(), line.size(), line.data(),
                     m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
            m_socket.close();
            return false;
        }
        else
        {
            std::string_view key{line.data(), index};
            std::string_view value{std::begin(line) + index + sep.size(), std::end(line)};

//            ESP_LOGD(TAG, "header key=\"%.*s\" value=\"%.*s\"", key.size(), key.data(), value.size(), value.data());

            if (cpputils::stringEqualsIgnoreCase(key, "Content-Length"))
            {
                if (const auto parsed = cpputils::fromString<std::size_t>(value); !parsed)
                {
                    ESP_LOGW(TAG, "invalid Content-Length %.*s %.*s", value.size(), value.data(),
                             parsed.error().size(), parsed.error().data());
                    m_socket.close();
                    return false;
                }
                else
                    m_requestBodySize = *parsed;
            }

            if (!m_responseHandler)
            {
                ESP_LOGW(TAG, "invalid response handler (%s:%hi)",
                         m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
                m_socket.close();
                return false;
            }

            m_responseHandler->requestHeaderReceived(key, value);
            return true;
        }
    }
    else
    {
        if (!m_responseHandler)
        {
            ESP_LOGW(TAG, "invalid response handler (%s:%hi)",
                     m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
            m_socket.close();
            return false;
        }

        if (m_requestBodySize)
        {
//            ESP_LOGV(TAG, "state changed to RequestBody");
            m_state = State::RequestBody;

            if (!m_parsingBuffer.empty())
            {
                if (m_parsingBuffer.size() <= m_requestBodySize)
                {
                    m_responseHandler->requestBodyReceived(m_parsingBuffer);
                    m_requestBodySize -= m_parsingBuffer.size();
                    m_parsingBuffer.clear();

                    if (!m_requestBodySize)
                        goto requestFinished;

                    return true;
                }
                else
                {
                    m_responseHandler->requestBodyReceived({m_parsingBuffer.data(), m_requestBodySize});
                    m_parsingBuffer.erase(std::begin(m_parsingBuffer), std::next(std::begin(m_parsingBuffer), m_requestBodySize));
                    m_requestBodySize = 0;
                    goto requestFinished;
                }
            }
            else
                return true;
        }
        else
        {
requestFinished:
//            ESP_LOGV(TAG, "state changed to Response");
            m_state = State::Response;

            m_responseHandler->sendResponse();

            return false;
        }
    }
}
