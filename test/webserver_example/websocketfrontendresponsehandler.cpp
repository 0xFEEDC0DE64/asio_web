#include "websocketfrontendresponsehandler.h"

// esp-idf includes
#include <asio.hpp>
#include <esp_log.h>

// 3rdparty lib includes
#include <fmt/core.h>
#include <asio_webserver/clientconnection.h>

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";
} // namespace

WebsocketFrontendResponseHandler::WebsocketFrontendResponseHandler(ClientConnection &clientConnection) :
    m_clientConnection{clientConnection}
{
    ESP_LOGI(TAG, "constructed for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

WebsocketFrontendResponseHandler::~WebsocketFrontendResponseHandler()
{
    ESP_LOGI(TAG, "destructed for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

void WebsocketFrontendResponseHandler::requestHeaderReceived(std::string_view key, std::string_view value)
{
}

void WebsocketFrontendResponseHandler::requestBodyReceived(std::string_view body)
{
}

void WebsocketFrontendResponseHandler::sendResponse()
{
    ESP_LOGI(TAG, "sending response for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

    m_response = R"END(
<!DOCTYPE html>
<html>
    <head>
        <title>Websocket test</title>
    </head>
    <body>
        <h1>Websocket test</h1>

        <div style="border: 1px solid black;">
            <button id="connectButton">Connect</button>
            <span id="statusSpan">Not connected</span>
        </div>

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

            const connectButton = document.getElementById('connectButton');
            const statusSpan = document.getElementById('statusSpan');
            const sendForm = document.getElementById('sendForm');
            const sendInput = document.getElementById('sendInput');
            const logOutput = document.getElementById('logOutput');

            document.addEventListener("DOMContentLoaded", function(event) {
                console.log('loaded');
                connectButton.addEventListener('click', connectButtonClicked);
                sendForm.addEventListener('submit', sendMsg);
            });

            function logLine(msg) {
                logOutput.appendChild(document.createTextNode(msg + "\n"));
            }

            function connectButtonClicked() {
                console.log('clicked');
                if (websocket === null) {
                    connectButton.textContent = 'Disconnect';

                    const url = (window.location.protocol === 'https:' ? 'wss://' : 'ws://') + window.location.host + '/ws';

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
                } else {
                    connectButton.textContent = 'Connect';

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
)END";

    m_response = fmt::format("HTTP/1.1 200 Ok\r\n"
                             "Connection: keep-alive\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: {}\r\n"
                             "\r\n"
                             "{}", m_response.size(), m_response);

    asio::async_write(m_clientConnection.socket(),
                      asio::buffer(m_response.data(), m_response.size()),
                      [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                      { written(ec, length); });
}

void WebsocketFrontendResponseHandler::written(std::error_code ec, std::size_t length)
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
