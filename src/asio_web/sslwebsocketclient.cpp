#include "sslwebsocketclient.h"

// esp-idf includes
#include <esp_log.h>

// 3rdparty lib includes
#include <strutils.h>
#include <numberparsing.h>
#include <fmt/core.h>

// local includes
#include "websocketstream.h"

namespace {
constexpr const char * const TAG = "ASIO_WEB";
} // namespace

SslWebsocketClient::SslWebsocketClient(asio::io_context &io_context, std::string &&host, std::string &&port, std::string &&path) :
    m_host(std::move(host)),
    m_port{std::move(port)},
    m_path{std::move(path)},
    m_resolver{io_context},
    m_socket{io_context, m_sslCtx}
    //m_socket{io_context}
{
    m_socket.set_verify_mode(asio::ssl::verify_none);
}

SslWebsocketClient::SslWebsocketClient(asio::io_context &io_context, const std::string &host, const std::string &port, const std::string &path) :
    m_host{host},
    m_port{port},
    m_path{path},
    m_resolver{io_context},
    m_socket{io_context, m_sslCtx}
    //m_socket{io_context}
{
    m_socket.set_verify_mode(asio::ssl::verify_none);
}

void SslWebsocketClient::start()
{
    ESP_LOGI(TAG, "called");
    resolve();
}

void SslWebsocketClient::resolve()
{
    ESP_LOGI(TAG, "called");

    m_resolver.async_resolve(m_host, m_port,
                             [this](const std::error_code &error, asio::ip::tcp::resolver::iterator iterator){
                                 onResolved(error, iterator);
                             });
//    m_resolver.async_resolve("ruezn.local", "1234",
//                             [this](const std::error_code &error, asio::ip::tcp::resolver::iterator iterator){
//                                 onResolved(error, iterator);
//                             });
}

void SslWebsocketClient::onResolved(const std::error_code &error, asio::ip::tcp::resolver::iterator iterator)
{
    if (error)
    {
        ESP_LOGW(TAG, "Resolving failed: %i %s", error.value(), error.message().c_str());
        m_error = Error { .mesage = fmt::format("Resolving failed: {}", error.value(), error.message()) };
        return;
    }

    ESP_LOGI(TAG, "called");

    connect(iterator);
}

void SslWebsocketClient::connect(const asio::ip::tcp::resolver::iterator &endpoints)
{
    ESP_LOGI(TAG, "called");

    asio::async_connect(m_socket.lowest_layer(), endpoints,
                        [this](const std::error_code & error, const asio::ip::tcp::resolver::iterator &) {
                            onConnected(error);
                        });
}

void SslWebsocketClient::onConnected(const std::error_code &error)
{
    if (error)
    {
        ESP_LOGW(TAG, "Connect failed: %i %s", error.value(), error.message().c_str());
        m_error = Error { .mesage = fmt::format("Connect failed: {}", error.value(), error.message()) };
        return;
    }

    ESP_LOGI(TAG, "called");

    handshake();
//    send_request();
}

void SslWebsocketClient::handshake()
{
    ESP_LOGI(TAG, "called");

    m_socket.async_handshake(asio::ssl::stream_base::client,
                             [this](const std::error_code &error) {
                                 onHandshaked(error);
                             });
}

void SslWebsocketClient::onHandshaked(const std::error_code &error)
{
    if (error)
    {
        ESP_LOGW(TAG, "SSL-Handshake failed: %i %s", error.value(), error.message().c_str());
        m_error = Error { .mesage = fmt::format("SSL-Handshake failed: {}", error.value(), error.message()) };
        return;
    }

    ESP_LOGI(TAG, "called");

    send_request();
}

void SslWebsocketClient::send_request()
{
    m_sending = fmt::format("GET {} HTTP/1.1\r\n"
                            "Host: {}\r\n"
                            "Connection: Upgrade\r\n"
                            "Upgrade: websocket\r\n"
                            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                            "Sec-WebSocket-Version: 13\r\n"
                            "\r\n", m_path, m_host);
    ESP_LOGI(TAG, "called %.*s", m_sending->size(), m_sending->data());

    m_state = State::Request;

    asio::async_write(m_socket,
                      asio::buffer(m_sending->data(), m_sending->size()),
                      [this](const std::error_code &error, std::size_t length) {
                          onSentRequest(error, length);
                      });
}

void SslWebsocketClient::onSentRequest(const std::error_code &error, std::size_t length)
{
    if (error)
    {
        ESP_LOGW(TAG, "Sending http request failed: %i %s", error.value(), error.message().c_str());
        m_error = Error { .mesage = fmt::format("Sending http request failed: {}", error.value(), error.message()) };
        m_sending = std::nullopt;
        m_socket.shutdown();
        return;
    }

    ESP_LOGI(TAG, "called %zd (%zd)", length, m_sending->size());

    m_sending = std::nullopt;
    m_state = State::ResponseLine;

    receive_response();
}

void SslWebsocketClient::receive_response()
{
    ESP_LOGI(TAG, "called");

    m_socket.async_read_some(asio::buffer(m_receiveBuffer, std::size(m_receiveBuffer)),
                             [this](const std::error_code &error, std::size_t length) {
                                 onReceivedResponse(error, length);
                             });
}

void SslWebsocketClient::onReceivedResponse(const std::error_code &error, std::size_t length)
{
    if (error)
    {
        ESP_LOGI(TAG, "Receiving http response failed: %i %s", error.value(), error.message().c_str());
        m_error = Error { .mesage = fmt::format("Receiving http response failed: {}", error.value(), error.message()) };
        m_socket.shutdown();
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

bool SslWebsocketClient::readyReadLine(std::string_view line)
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

bool SslWebsocketClient::parseResponseLine(std::string_view line)
{
//    ESP_LOGV(TAG, "%.*s", line.size(), line.data());

    if (const auto index = line.find(' '); index == std::string::npos)
    {
        ESP_LOGW(TAG, "invalid response line (1): \"%.*s\"", line.size(), line.data());
        m_error = Error { .mesage = fmt::format("invalid response line (1): \"{}\"", line) };
        m_socket.shutdown();
        return false;
    }
    else
    {
        const std::string_view protocol { line.data(), index };
//        ESP_LOGV(TAG, "response protocol: %zd \"%.*s\"", protocol.size(), protocol.size(), protocol.data());

        if (const auto index2 = line.find(' ', index + 1); index2 == std::string::npos)
        {
            ESP_LOGW(TAG, "invalid request line (2): \"%.*s\"", line.size(), line.data());
            m_error = Error { .mesage = fmt::format("invalid response line (2): \"{}\"", line) };
            m_socket.shutdown();
            return false;
        }
        else
        {
            const std::string_view status { line.data() + index + 1, line.data() + index2 };
//            ESP_LOGV(TAG, "response status: %zd \"%.*s\"", status.size(), status.size(), status.data());

            const std::string_view message { line.cbegin() + index2 + 1, line.cend() };
//            ESP_LOGV(TAG, "response message: %zd \"%.*s\"", message.size(), message.size(), message.data());

//            ESP_LOGV(TAG, "state changed to ResponseHeaders");
            m_state = State::ResponseHeaders;

            return true;
        }
    }
}

bool SslWebsocketClient::parseResponseHeader(std::string_view line)
{
//    ESP_LOGV(TAG, "%.*s", line.size(), line.data());

    if (!line.empty())
    {
        constexpr std::string_view sep{": "};
        if (const auto index = line.find(sep.data(), 0, sep.size()); index == std::string_view::npos)
        {
            ESP_LOGW(TAG, "invalid response header: %zd \"%.*s\"", line.size(), line.size(), line.data());
            m_error = Error { .mesage = fmt::format("invalid response header: \"{}\"", line) };
            m_socket.shutdown();
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
                    m_error = Error { .mesage = fmt::format("iinvalid Content-Length: \"{}\": {}", value, parsed.error()) };
                    m_socket.shutdown();
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
//            ESP_LOGV(TAG, "finished");

            handleConnected();

//            ESP_LOGV(TAG, "state changed to WebSocket");
            m_state = State::WebSocket;

//            m_responseHandler->sendResponse();

            return true;
        }
    }
}

void SslWebsocketClient::doReadWebSocket()
{
    ESP_LOGI(TAG, "called");

    m_socket.async_read_some(asio::buffer(m_receiveBuffer, std::size(m_receiveBuffer)),
                             [this](const std::error_code &error, std::size_t length) {
                                 onReceiveWebsocket(error, length);
                             });
}

void SslWebsocketClient::onReceiveWebsocket(const std::error_code &error, std::size_t length)
{
    if (error)
    {
        ESP_LOGI(TAG, "Receiving websocket response failed: %i %s", error.value(), error.message().c_str());
        m_error = Error { .mesage = fmt::format("Receiving websocket response failed: {}", error.value(), error.message()) };
        m_socket.shutdown();
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

    ESP_LOGI(TAG, "fin=%i reserved=%i opcode=%i mask=%i payloadLength=%i", hdr.fin, hdr.reserved, hdr.opcode, hdr.mask, hdr.payloadLength);

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

        ESP_LOGI(TAG, "16bit payloadLength: %u", payloadLength);
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

    ESP_LOGI(TAG, "remaining: std::distance=%zd payloadLength=%llu", std::distance(iter, std::end(m_parsingBuffer)), payloadLength);

    std::string_view payload{&*iter, (unsigned int)(payloadLength)};

    ESP_LOGI(TAG, "payload: %.*s", payload.size(), payload.data());

    handleMessage(hdr.fin, hdr.reserved, hdr.opcode, hdr.mask, payload);

    std::advance(iter, payloadLength);
    m_parsingBuffer.erase(std::begin(m_parsingBuffer), iter);

    goto again;
}

void SslWebsocketClient::sendMessage(bool fin, uint8_t reserved, uint8_t opcode, bool mask, std::string_view payload)
{
    //ESP_LOGI(TAG, "%.*s", payload.size(), payload.data());

    std::string sendBuffer;
    sendBuffer.resize(2);
    {
        auto iter = std::begin(sendBuffer);
        WebsocketHeader &hdr = *(WebsocketHeader *)(&*iter);
        hdr.fin = fin;
        hdr.reserved = reserved;
        hdr.opcode = opcode;
        hdr.mask = mask;
        hdr.payloadLength = payload.size() < 126 ? payload.size() : 126;
    }
    if (payload.size() > 125)
    {
        union {
            char buf[2];
            uint16_t length;
        };
        length = __builtin_bswap16((uint16_t)payload.size());
        sendBuffer.append(std::string_view{buf, 2});
    }
    if (mask)
    {
        sendBuffer.append(4, '\0');
    }
    sendBuffer.append(payload);

    if (!m_sending)
    {
        m_sending = std::move(sendBuffer);

        asio::async_write(m_socket,
                          asio::buffer(m_sending->data(), m_sending->size()),
                          [this](std::error_code ec, std::size_t length)
                          { onMessageSent(ec, length); });
    }
    else
    {
        m_sendingQueue.push(std::move(sendBuffer));

        ESP_LOGI(TAG, "enqueueing %zd", m_sendingQueue.size());
    }
}

void SslWebsocketClient::onMessageSent(std::error_code error, std::size_t length)
{
    if (error)
    {
        ESP_LOGI(TAG, "Sending websocket message failed: %i %s", error.value(), error.message().c_str());
        m_error = Error { .mesage = fmt::format("Sending websocket message failed: {}", error.value(), error.message()) };
        m_socket.shutdown();
        m_sending = std::nullopt;
        m_sendingQueue = {};
        return;
    }

//    ESP_LOGI(TAG, "length=%zd expected=%zd", length, m_sending->size());

    if (m_sendingQueue.empty())
    {
        m_sending = std::nullopt;
    }
    else
    {
        m_sending = m_sendingQueue.front();
        m_sendingQueue.pop();

//        ESP_LOGI(TAG, "asio send %zd %.*s", m_sending->size(), (int)m_sending->size(), m_sending->data());

        asio::async_write(m_socket,
                          asio::buffer(m_sending->data(), m_sending->size()),
                          [this](std::error_code ec, std::size_t length)
                          { onMessageSent(ec, length); });
    }
}
