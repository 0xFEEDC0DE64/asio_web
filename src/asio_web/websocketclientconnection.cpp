#include "websocketclientconnection.h"

// system includes

// esp-idf includes
#include <esp_log.h>

// 3rdparty lib includes
#include <strutils.h>
#include <fmt/core.h>

// local includes
#include "webserver.h"
#include "responsehandler.h"
#include "websocketstream.h"

namespace {
constexpr const char * const TAG = "ASIO_WEB";
} // namespace

WebsocketClientConnection::WebsocketClientConnection(Webserver &webserver, asio::ip::tcp::socket socket,
                                                     std::string &&parsingBuffer, std::unique_ptr<ResponseHandler> &&responseHandler) :
    m_webserver{webserver},
    m_socket{std::move(socket)},
    m_remote_endpoint{m_socket.remote_endpoint()},
    m_parsingBuffer{std::move(parsingBuffer)},
    m_responseHandler{std::move(responseHandler)}
{
    ESP_LOGI(TAG, "new client (%s:%hi)",
             m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());

    m_webserver.m_websocketClients++;
}

WebsocketClientConnection::~WebsocketClientConnection()
{
    ESP_LOGI(TAG, "client destroyed (%s:%hi)",
             m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());

    m_webserver.m_websocketClients--;
}

void WebsocketClientConnection::start()
{
    doReadWebSocket();
}

void WebsocketClientConnection::doReadWebSocket()
{
    m_socket.async_read_some(asio::buffer(m_receiveBuffer, max_length),
                             [this, self=shared_from_this()](std::error_code ec, std::size_t length)
                             { readyReadWebSocket(ec, length); });
}

void WebsocketClientConnection::readyReadWebSocket(std::error_code ec, std::size_t length)
{
    if (ec)
    {
        ESP_LOGI(TAG, "error: %i %s (%s:%hi)", ec.value(), ec.message().c_str(),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
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

        ESP_LOGI(TAG, "64bit payloadLength: %llu", payloadLength);
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

    ESP_LOGI(TAG, "remaining: %zd %llu", std::distance(iter, std::end(m_parsingBuffer)), payloadLength);

    ESP_LOGI(TAG, "payload: %.*s", int(payloadLength), &*iter);

    std::advance(iter, payloadLength);
    m_parsingBuffer.erase(std::begin(m_parsingBuffer), iter);

    sendMessage(true, 0, 1, false, fmt::format("received {}", payloadLength));

    goto again;
}

void WebsocketClientConnection::sendMessage(bool fin, uint8_t reserved, uint8_t opcode, bool mask, std::string_view payload)
{
    m_sendBuffer.clear();
    m_sendBuffer.resize(2);
    {
        auto iter = std::begin(m_sendBuffer);
        WebsocketHeader &hdr = *(WebsocketHeader *)(&*iter);
        hdr.fin = fin;
        hdr.reserved = reserved;
        hdr.opcode = opcode;
        hdr.mask = mask;
        hdr.payloadLength = payload.size();
    }
    m_sendBuffer.append(payload);

    asio::async_write(m_socket,
                      asio::buffer(m_sendBuffer.data(), m_sendBuffer.size()),
                      [this](std::error_code ec, std::size_t length)
                      { onMessageSent(ec, length); });
}

void WebsocketClientConnection::onMessageSent(std::error_code ec, std::size_t length)
{
    if (ec)
    {
        ESP_LOGW(TAG, "error: %i (%s:%hi)", ec.value(),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        return;
    }

    ESP_LOGI(TAG, "length=%zd", length);
}
