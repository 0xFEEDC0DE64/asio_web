#include "websocketresponsehandler.h"

// system includes
#include <openssl/sha.h>

// esp-idf includes
#include <asio.hpp>
#include <esp_log.h>

// 3rdparty lib includes
#include <fmt/core.h>
#include <asio_webserver/clientconnection.h>
#include <asio_webserver/webserver.h>
#include <strutils.h>

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";

constexpr std::string_view html{R"END(
<!DOCTYPE html>
<html>
    <head>
        <title>Websocket test</title>
    </head>
    <body>
        <h1>Websocket test</h1>

        <form id="connectForm">
            <fieldset>
                <legend>Connection</legend>
                <input type="url" id="urlInput" required />
                <button id="connectButton" type="submit">Connect</button>
                <span id="statusSpan">Not connected</span>
            </fieldset>
        </form>

        <form id="sendForm">
            <fieldset>
                <legend>Send msg</legend>
                <input type="text" id="sendInput" />
                <button type="submit">Send</button>
            </fieldset>
        </form>

        <pre id="logOutput"></pre>

        <script>
            var websocket = null;

            const connectForm = document.getElementById('connectForm');
            const urlInput = document.getElementById('urlInput');
            const connectButton = document.getElementById('connectButton');
            const statusSpan = document.getElementById('statusSpan');
            const sendForm = document.getElementById('sendForm');
            const sendInput = document.getElementById('sendInput');
            const logOutput = document.getElementById('logOutput');

            document.addEventListener("DOMContentLoaded", function(event) {
                urlInput.value = (window.location.protocol === 'https:' ? 'wss://' : 'ws://') + window.location.host + '/ws'

                connectForm.addEventListener('submit', connectWebsocket);
                sendForm.addEventListener('submit', sendMsg);
            });

            function logLine(msg) {
                logOutput.appendChild(document.createTextNode(msg + "\n"));
            }

            function connectWebsocket(ev) {
                ev.preventDefault();

                if (websocket === null) {
                    const url = urlInput.value;

                    logLine('Connecting to ' + url);

                    statusSpan.textContent = "Connecting...";

                    websocket = new WebSocket(url);
                    websocket.onopen = function (event) {
                        statusSpan.textContent = "Connected";
                        logLine('Connected');
                    };
                    websocket.onclose = function(event) {
                        statusSpan.textContent = "Lost connection";
                        logLine('Lost connection');
                    };
                    websocket.onerror = function(event) {
                        statusSpan.textContent = "Error occured";
                        logLine('Error occured');
                    };
                    websocket.onmessage = function(event) {
                        if (typeof event.data === 'string' || event.data instanceof String) {
                            logLine('Received text message: ' + event.data);
                        } else if (typeof event.data == 'object') {
                            logLine('Received binary message');
                        } else {
                            logLine('Received unknown message');
                        }
                    };

                    connectButton.textContent = 'Disconnect';
                    urlInput.readOnly = true;
                } else {
                    connectButton.textContent = 'Connect';
                    urlInput.readOnly = false;

                    websocket.close();
                    websocket = null;
                }
            }

            function sendMsg(ev) {
                ev.preventDefault();

                if (websocket === null) {
                    alert('not connected!');
                    return;
                }

                websocket.send(sendInput.value);
                logLine('Sent text message: ' + sendInput.value);
                sendInput.value = '';
                sendInput.focus();
            }
        </script>
    </body>
</html>
)END"};
} // namespace

WebsocketResponseHandler::WebsocketResponseHandler(ClientConnection &clientConnection) :
    m_clientConnection{clientConnection}
{
//    ESP_LOGV(TAG, "constructed for (%s:%hi)",
//             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

WebsocketResponseHandler::~WebsocketResponseHandler()
{
//    ESP_LOGV(TAG, "destructed for (%s:%hi)",
//             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

void WebsocketResponseHandler::requestHeaderReceived(std::string_view key, std::string_view value)
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

void WebsocketResponseHandler::requestBodyReceived(std::string_view body)
{
}

void WebsocketResponseHandler::sendResponse()
{
    ESP_LOGI(TAG, "sending response for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

    if (!m_connectionUpgrade || !m_upgradeWebsocket)
    {
        m_response = fmt::format("HTTP/1.1 200 Ok\r\n"
                                 "Connection: {}\r\n"
                                 "Content-Type: text/html\r\n"
                                 "Content-Length: {}\r\n"
                                 "\r\n",
                                 m_clientConnection.webserver().connectionKeepAlive() ? "keep-alive" : "close",
                                 html.size());

        asio::async_write(m_clientConnection.socket(),
                          asio::buffer(m_response.data(), m_response.size()),
                          [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                          { writtenHtmlHeader(ec, length); });

        return;
    }

    const auto showError = [&](std::string_view msg){
        m_response = fmt::format("HTTP/1.1 400 Bad Request\r\n"
                                 "Connection: {}\r\n"
                                 "Content-Type: text/html\r\n"
                                 "Content-Length: {}\r\n"
                                 "\r\n"
                                 "{}",
                                 m_clientConnection.webserver().connectionKeepAlive() ? "keep-alive" : "close",
                                 msg.size(), msg);

        asio::async_write(m_clientConnection.socket(),
                          asio::buffer(m_response.data(), m_response.size()),
                          [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                          { writtenHtml(ec, length); });
    };

    if (m_secWebsocketKey.empty())
    {
        showError("Header Sec-WebSocket-Key empty or missing!");
        return;
    }

    constexpr std::string_view magic_uuid{"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"};
    m_secWebsocketKey.append(magic_uuid);

    unsigned char sha1[SHA_DIGEST_LENGTH]; // == 20
    SHA1((const unsigned char *)m_secWebsocketKey.data(), m_secWebsocketKey.size(), sha1);

    const auto base64Sha1 = cpputils::toBase64String({sha1, SHA_DIGEST_LENGTH});

    m_response = fmt::format("HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: {}\r\n"
                             "\r\n", base64Sha1);

    asio::async_write(m_clientConnection.socket(),
                      asio::buffer(m_response.data(), m_response.size()),
                      [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                      { writtenWebsocket(ec, length); });
}

void WebsocketResponseHandler::writtenHtmlHeader(std::error_code ec, std::size_t length)
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

    asio::async_write(m_clientConnection.socket(),
                      asio::buffer(html.data(), html.size()),
                      [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                      { writtenHtml(ec, length); });
}

void WebsocketResponseHandler::writtenHtml(std::error_code ec, std::size_t length)
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

void WebsocketResponseHandler::writtenWebsocket(std::error_code ec, std::size_t length)
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
