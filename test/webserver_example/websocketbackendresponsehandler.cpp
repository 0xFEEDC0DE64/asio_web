#include "websocketbackendresponsehandler.h"

// system includes
#include <openssl/sha.h>

// esp-idf includes
#include <asio.hpp>
#include <esp_log.h>

// 3rdparty lib includes
#include <fmt/core.h>
#include <asio_webserver/clientconnection.h>
#include <strutils.h>

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";
} // namespace

WebsocketBackendResponseHandler::WebsocketBackendResponseHandler(ClientConnection &clientConnection) :
    m_clientConnection{clientConnection}
{
    ESP_LOGI(TAG, "constructed for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

WebsocketBackendResponseHandler::~WebsocketBackendResponseHandler()
{
    ESP_LOGI(TAG, "destructed for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

void WebsocketBackendResponseHandler::requestHeaderReceived(std::string_view key, std::string_view value)
{
//    ESP_LOGV(TAG, "key=\"%.*s\" value=\"%.*s\"", key.size(), key.data(), value.size(), value.data());

    if (cpputils::stringEqualsIgnoreCase(key, "Connection"))
    {
        m_connectionUpgrade = cpputils::stringEqualsIgnoreCase(value, "Upgrade") ||
            value.contains("Upgrade");
    }
    else if (cpputils::stringEqualsIgnoreCase(key, "Upgrade"))
    {
        m_upgradeWebsocket = cpputils::stringEqualsIgnoreCase(value, "websocket");
    }
    else if (cpputils::stringEqualsIgnoreCase(key, "Sec-WebSocket-Version"))
    {
        m_secWebsocketVersion = value;
    }
    else if (cpputils::stringEqualsIgnoreCase(key, "Sec-WebSocket-Key"))
    {
        m_secWebsocketKey = value;
    }
    else if (cpputils::stringEqualsIgnoreCase(key, "Sec-WebSocket-Extensions"))
    {
        m_secWebsocketExtensions = value;
    }
}

void WebsocketBackendResponseHandler::requestBodyReceived(std::string_view body)
{
}

void WebsocketBackendResponseHandler::sendResponse()
{
    ESP_LOGI(TAG, "sending response for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

    const auto sendErrorResponse = [&](std::string_view message){
        ESP_LOGW(TAG, "%.*s", message.size(), message.data());

        m_response = fmt::format("HTTP/1.1 400 Bad Request\r\n"
                                 "Connection: keep-alive\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: {}\r\n"
                                 "\r\n"
                                 "{}", message.size(), message);

        asio::async_write(m_clientConnection.socket(),
                          asio::buffer(m_response.data(), m_response.size()),
                          [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                          { writtenError(ec, length); });
    };

    if (!m_connectionUpgrade)
    {
        sendErrorResponse("Only websocket clients are allowed on this endpoint (header missing Connection: Upgrade)");
        return;
    }

    if (!m_upgradeWebsocket)
    {
        sendErrorResponse("Only websocket clients are allowed on this endpoint (header missing Upgrade: websocket)");
        return;
    }

    if (m_secWebsocketKey.empty())
    {
        sendErrorResponse("Only websocket clients are allowed on this endpoint (header missing Sec-WebSocket-Key)");
        return;
    }

    constexpr std::string_view magic_uuid{"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"};
    m_secWebsocketKey.append(magic_uuid);

    unsigned char hash[SHA_DIGEST_LENGTH]; // == 20
    SHA1((const unsigned char *)m_secWebsocketKey.data(), m_secWebsocketKey.size(), hash);

    const auto base64Hash = cpputils::toBase64String({hash, SHA_DIGEST_LENGTH});

    m_response = fmt::format("HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: {}\r\n"
                             "Sec-WebSocket-Protocol: chat\r\n"
                             "\r\n", base64Hash);

    asio::async_write(m_clientConnection.socket(),
                      asio::buffer(m_response.data(), m_response.size()),
                      [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                      { written(ec, length); });
}

void WebsocketBackendResponseHandler::writtenError(std::error_code ec, std::size_t length)
{
    if (ec)
    {
        ESP_LOGW(TAG, "error: %i (%s:%hi)", ec.value(),
                 m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
        m_clientConnection.responseFinished(ec);
        return;
    }

    ESP_LOGI(TAG, "expected=%zd actual=%zd for (%s:%hi)", m_response.size(), length,
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

    m_clientConnection.responseFinished(ec);
}

void WebsocketBackendResponseHandler::written(std::error_code ec, std::size_t length)
{
    if (ec)
    {
        ESP_LOGW(TAG, "error: %i (%s:%hi)", ec.value(),
                 m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
        m_clientConnection.responseFinished(ec);
        return;
    }

    ESP_LOGI(TAG, "expected=%zd actual=%zd for (%s:%hi)", m_response.size(), length,
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

    m_clientConnection.upgradeWebsocket();
}
