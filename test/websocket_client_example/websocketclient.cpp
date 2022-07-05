#include "websocketclient.h"

// esp-idf includes
#include <esp_log.h>

// 3rdparty lib includes
#include <strutils.h>
#include <numberparsing.h>

// local includes
#include "asio_web/websocketstream.h"

namespace {

const std::string_view request = "GET /charger/99999999 HTTP/1.1\r\n"
                                 "Host: localhost\r\n"
                                 "Connection: Upgrade\r\n"
                                 "Upgrade: websocket\r\n"
                                 "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                                 "Sec-WebSocket-Version: 13\r\n"
                                 "\r\n";

} // namespace

WebsocketClient::WebsocketClient(asio::io_context &io_context) :
    m_resolver{io_context},
    //m_socket{io_context, m_sslCtx},
    m_socket{io_context}
{
    //m_socket.set_verify_mode(asio::ssl::verify_none);
}

void WebsocketClient::start()
{
    resolve();
}

void WebsocketClient::resolve()
{
    ESP_LOGI(TAG, "called");

//    m_resolver.async_resolve("backend.com", "8086",
//                             [this](const std::error_code &error, asio::ip::tcp::resolver::iterator iterator){
//                                 onResolved(error, iterator);
//                             });
    m_resolver.async_resolve("localhost", "1234",
                             [this](const std::error_code &error, asio::ip::tcp::resolver::iterator iterator){
                                 onResolved(error, iterator);
                             });
}

void WebsocketClient::onResolved(const std::error_code &error, asio::ip::tcp::resolver::iterator iterator)
{
    if (error)
    {
        ESP_LOGW(TAG, "Resolving failed: %s", error.message().c_str());
        return;
    }

    connect(iterator);
}

void WebsocketClient::connect(const asio::ip::tcp::resolver::iterator &endpoints)
{
    ESP_LOGI(TAG, "called");

    asio::async_connect(m_socket.lowest_layer(), endpoints,
                        [this](const std::error_code & error, const asio::ip::tcp::resolver::iterator &) {
                            onConnected(error);
                        });
}

void WebsocketClient::onConnected(const std::error_code &error)
{
    if (error)
    {
        ESP_LOGW(TAG, "Connect failed: %s", error.message().c_str());
        return;
    }

//    handshake();
    send_request();
}

void WebsocketClient::handshake()
{
    ESP_LOGI(TAG, "called");

//    m_socket.async_handshake(asio::ssl::stream_base::client,
//                            [this](const std::error_code &error) {
//                                onHandshaked(error);
//                            });
}

void WebsocketClient::onHandshaked(const std::error_code &error)
{
    if (error)
    {
        ESP_LOGW(TAG, "Handshake failed: %s", error.message().c_str());
        return;
    }

    send_request();
}

void WebsocketClient::send_request()
{
    ESP_LOGI(TAG, "called %.*s", request.size(), request.data());

    m_state = State::Request;

    asio::async_write(m_socket,
                      asio::buffer(request.data(), request.size()),
                      [this](const std::error_code &error, std::size_t length) {
                          onSentRequest(error, length);
                      });
}

void WebsocketClient::onSentRequest(const std::error_code &error, std::size_t length)
{
    if (error)
    {
        ESP_LOGW(TAG, "Write failed: %s", error.message().c_str());
        return;
    }

    ESP_LOGI(TAG, "called %zd (%zd)", length, request.size());

    m_state = State::ResponseLine;

    receive_response();
}

void WebsocketClient::receive_response()
{
    ESP_LOGI(TAG, "called");

    m_socket.async_read_some(asio::buffer(m_receiveBuffer, std::size(m_receiveBuffer)),
                            [this](const std::error_code &error, std::size_t length) {
                                onReceivedResponse(error, length);
                            });
}

void WebsocketClient::onReceivedResponse(const std::error_code &error, std::size_t length)
{
    if (error)
    {
        ESP_LOGI(TAG, "Read failed: %s", error.message().c_str());
        return;
    }

    ESP_LOGI(TAG, "received %.*s", length, m_receiveBuffer);
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
        if (m_state == State::WebSocket)
            break;
    }

    if (shouldDoRead)
    {
        if (m_state == State::WebSocket)
            doReadWebSocket();
        else
            receive_response();
    }
}

bool WebsocketClient::readyReadLine(std::string_view line)
{
    switch (m_state)
    {
    case State::Request:
//        ESP_LOGV(TAG, "case State::Request:");
        ESP_LOGW(TAG, "unexpected state=Request");
        return true;
    case State::ResponseLine:
//        ESP_LOGV(TAG, "case State::StatusLine:");
        return parseResponseLine(line);
    case State::ResponseHeaders:
//        ESP_LOGV(TAG, "case State::ResponseHeaders:");
        return parseResponseHeader(line);
    case State::ResponseBody:
//        ESP_LOGV(TAG, "case State::RequestBody:");
        ESP_LOGW(TAG, "unexpected state=ResponseBody");
        return true;
    default:
        ESP_LOGW(TAG, "unknown state %i", std::to_underlying(m_state));
        return true;
    }
}

bool WebsocketClient::parseResponseLine(std::string_view line)
{
//    ESP_LOGV(TAG, "%.*s", line.size(), line.data());

    if (const auto index = line.find(' '); index == std::string::npos)
    {
        ESP_LOGW(TAG, "invalid response line (1): \"%.*s\"", line.size(), line.data());
        //m_socket.close();
        return false;
    }
    else
    {
        const std::string_view protocol { line.data(), index };
        ESP_LOGV(TAG, "response protocol: %zd \"%.*s\"", protocol.size(), protocol.size(), protocol.data());

        if (const auto index2 = line.find(' ', index + 1); index2 == std::string::npos)
        {
            ESP_LOGW(TAG, "invalid request line (2): \"%.*s\"", line.size(), line.data());
            //m_socket.close();
            return false;
        }
        else
        {
            const std::string_view status { line.data() + index + 1, line.data() + index2 };
            ESP_LOGV(TAG, "response status: %zd \"%.*s\"", status.size(), status.size(), status.data());

            const std::string_view message { line.cbegin() + index2 + 1, line.cend() };
            ESP_LOGV(TAG, "response message: %zd \"%.*s\"", message.size(), message.size(), message.data());

//            ESP_LOGV(TAG, "state changed to ResponseHeaders");
            m_state = State::ResponseHeaders;

            return true;
        }
    }
}

bool WebsocketClient::parseResponseHeader(std::string_view line)
{
//    ESP_LOGV(TAG, "%.*s", line.size(), line.data());

    if (!line.empty())
    {
        constexpr std::string_view sep{": "};
        if (const auto index = line.find(sep.data(), 0, sep.size()); index == std::string_view::npos)
        {
            ESP_LOGW(TAG, "invalid request header: %zd \"%.*s\"", line.size(), line.size(), line.data());
            //m_socket.close();
            return false;
        }
        else
        {
            std::string_view key{line.data(), index};
            std::string_view value{std::begin(line) + index + sep.size(), std::end(line)};

            ESP_LOGD(TAG, "header key=\"%.*s\" value=\"%.*s\"", key.size(), key.data(), value.size(), value.data());

            if (cpputils::stringEqualsIgnoreCase(key, "Content-Length"))
            {
                if (const auto parsed = cpputils::fromString<std::size_t>(value); !parsed)
                {
                    ESP_LOGW(TAG, "invalid Content-Length %.*s %.*s", value.size(), value.data(),
                             parsed.error().size(), parsed.error().data());
                    //m_socket.close();
                    return false;
                }
                else
                    m_responseBodySize = *parsed;
            }

            return true;
        }
    }
    else
    {
        if (m_responseBodySize)
        {
//            ESP_LOGV(TAG, "state changed to ResponseBody");
            m_state = State::ResponseBody;

            if (!m_parsingBuffer.empty())
            {
                if (m_parsingBuffer.size() <= m_responseBodySize)
                {
//                    m_responseHandler->requestBodyReceived(m_parsingBuffer);
                    m_responseBodySize -= m_parsingBuffer.size();
                    m_parsingBuffer.clear();

                    if (!m_responseBodySize)
                        goto requestFinished;

                    return true;
                }
                else
                {
//                    m_responseHandler->requestBodyReceived({m_parsingBuffer.data(), m_responseBodySize});
                    m_parsingBuffer.erase(std::begin(m_parsingBuffer), std::next(std::begin(m_parsingBuffer), m_responseBodySize));
                    m_responseBodySize = 0;
                    goto requestFinished;
                }
            }
            else
                return true;
        }
        else
        {
        requestFinished:
            ESP_LOGI(TAG, "finished");

//            ESP_LOGV(TAG, "state changed to WebSocket");
            m_state = State::WebSocket;

//            m_responseHandler->sendResponse();

            return true;
        }
    }
}

void WebsocketClient::doReadWebSocket()
{
    ESP_LOGI(TAG, "called");

    m_socket.async_read_some(asio::buffer(m_receiveBuffer, std::size(m_receiveBuffer)),
                             [this](const std::error_code &error, std::size_t length) {
                                 onReceiveWebsocket(error, length);
                             });
}

void WebsocketClient::onReceiveWebsocket(const std::error_code &error, std::size_t length)
{
    if (error)
    {
        ESP_LOGI(TAG, "error: %i %s", error.value(), error.message().c_str());
        return;
    }

//    ESP_LOGV(TAG, "received: %zd \"%.*s\"", length, length, m_receiveBuffer);

    m_parsingBuffer.append({m_receiveBuffer, length});

again:
    //    ESP_LOGV(TAG, "m_parsingBuffer: %s", cpputils::toHexString(m_parsingBuffer).c_str());

    if (m_parsingBuffer.empty())
    {
        doReadWebSocket();
        return;
    }

    static_assert(sizeof(WebsocketHeader) == 2);

    if (m_parsingBuffer.size() < sizeof(WebsocketHeader))
    {
        ESP_LOGW(TAG, "buffer smaller than a websocket header");
        doReadWebSocket();
        return;
    }

    //    ESP_LOGV(TAG, "%s%s%s%s %s%s%s%s    %s%s%s%s %s%s%s%s",
    //                  m_parsingBuffer.data()[0]&128?"1":".", m_parsingBuffer.data()[0]&64?"1":".", m_parsingBuffer.data()[0]&32?"1":".", m_parsingBuffer.data()[0]&16?"1":".",
    //                  m_parsingBuffer.data()[0]&8?"1":".", m_parsingBuffer.data()[0]&4?"1":".", m_parsingBuffer.data()[0]&2?"1":".", m_parsingBuffer.data()[0]&1?"1":".",
    //                  m_parsingBuffer.data()[1]&128?"1":".", m_parsingBuffer.data()[1]&64?"1":".", m_parsingBuffer.data()[1]&32?"1":".", m_parsingBuffer.data()[1]&16?"1":".",
    //                  m_parsingBuffer.data()[1]&8?"1":".", m_parsingBuffer.data()[1]&4?"1":".", m_parsingBuffer.data()[1]&2?"1":".", m_parsingBuffer.data()[1]&1?"1":".");

    auto iter = std::begin(m_parsingBuffer);

    const WebsocketHeader &hdr = *(const WebsocketHeader *)(&*iter);
    std::advance(iter, sizeof(WebsocketHeader));

    //    ESP_LOGV(TAG, "fin=%i reserved=%i opcode=%i mask=%i payloadLength=%i", hdr.fin, hdr.reserved, hdr.opcode, hdr.mask, hdr.payloadLength);

    uint64_t payloadLength = hdr.payloadLength;

    if (hdr.payloadLength == 126)
    {
        if (std::distance(iter, std::end(m_parsingBuffer)) < sizeof(uint16_t))
        {
            ESP_LOGW(TAG, "buffer smaller than uint32_t payloadLength");
            doReadWebSocket();
            return;
        }

        payloadLength = __builtin_bswap16(*(const uint16_t *)(&*iter));
        std::advance(iter, sizeof(uint16_t));

        //        ESP_LOGV(TAG, "16bit payloadLength: %u", payloadLength);
    }
    else if (hdr.payloadLength == 127)
    {
        if (std::distance(iter, std::end(m_parsingBuffer)) < sizeof(uint64_t))
        {
            ESP_LOGW(TAG, "buffer smaller than uint64_t payloadLength");
            doReadWebSocket();
            return;
        }

        payloadLength = *(const uint64_t *)(&*iter);
        std::advance(iter, sizeof(uint64_t));

        ESP_LOGI(TAG, "64bit payloadLength: %u", payloadLength);
    }

    if (hdr.mask)
    {
        if (std::distance(iter, std::end(m_parsingBuffer)) < sizeof(uint32_t))
        {
            ESP_LOGW(TAG, "buffer smaller than uint32_t mask");
            doReadWebSocket();
            return;
        }

        union {
            uint32_t mask;
            uint8_t maskArr[4];
        };
        mask = *(const uint32_t *)(&*iter);
        std::advance(iter, sizeof(uint32_t));

        if (std::distance(iter, std::end(m_parsingBuffer)) < payloadLength)
        {
            ESP_LOGW(TAG, "masked buffer smaller payloadLength");
            doReadWebSocket();
            return;
        }

        auto iter2 = std::begin(maskArr);
        for (auto iter3 = iter;
             iter3 != std::end(m_parsingBuffer) && iter3 != std::next(iter, payloadLength);
             iter3++)
        {
            *iter3 ^= *(iter2++);
            if (iter2 == std::end(maskArr))
                iter2 = std::begin(maskArr);
        }
    }
    else if (std::distance(iter, std::end(m_parsingBuffer)) < payloadLength)
    {
        ESP_LOGW(TAG, "buffer smaller payloadLength");
        doReadWebSocket();
        return;
    }

    ESP_LOGI(TAG, "remaining: %zd %lu", std::distance(iter, std::end(m_parsingBuffer)), payloadLength);

    ESP_LOGI(TAG, "payload: %.*s", payloadLength, &*iter);

    std::advance(iter, payloadLength);
    m_parsingBuffer.erase(std::begin(m_parsingBuffer), iter);

    sendMessage(true, 0, 1, true, "{\"type\":\"hello\"}");

    goto again;
}

void WebsocketClient::sendMessage(bool fin, uint8_t reserved, uint8_t opcode, bool mask, std::string_view payload)
{
    ESP_LOGI(TAG, "%.*s", payload.size(), payload.data());

    auto sendBuffer = std::make_shared<std::string>();
    sendBuffer->resize(2);
    {
        auto iter = std::begin(*sendBuffer);
        WebsocketHeader &hdr = *(WebsocketHeader *)(&*iter);
        hdr.fin = fin;
        hdr.reserved = reserved;
        hdr.opcode = opcode;
        hdr.mask = mask;
        hdr.payloadLength = payload.size();
    }
    if (mask)
    {
        sendBuffer->append(4, '\0');
        ESP_LOGI(TAG, "sendBuffer size %zd", sendBuffer->size());
        assert(sendBuffer->size() == 6);
    }
    sendBuffer->append(payload);

    asio::async_write(m_socket,
                      asio::buffer(sendBuffer->data(), sendBuffer->size()),
                      [this, sendBuffer](std::error_code ec, std::size_t length)
                      { onMessageSent(ec, length, sendBuffer->size()); });
}

void WebsocketClient::onMessageSent(std::error_code ec, std::size_t length, std::size_t expectedLength)
{
    if (ec)
    {
        ESP_LOGW(TAG, "error: %i", ec.value());
        return;
    }

    ESP_LOGI(TAG, "length=%zd expected=%zd", length, expectedLength);
}
