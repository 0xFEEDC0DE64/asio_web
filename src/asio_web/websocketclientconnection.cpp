#include "websocketclientconnection.h"

// system includes

// esp-idf includes
#include <esp_log.h>

// 3rdparty lib includes
#include <strutils.h>

// local includes
#include "webserver.h"
#include "responsehandler.h"

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";
} // namespace

WebsocketClientConnection::WebsocketClientConnection(Webserver &webserver, asio::ip::tcp::socket socket, std::unique_ptr<ResponseHandler> &&responseHandler) :
    m_webserver{webserver},
    m_socket{std::move(socket)},
    m_remote_endpoint{m_socket.remote_endpoint()},
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
    ESP_LOGI(TAG, "m_parsingBuffer: %s", cpputils::toHexString(m_parsingBuffer).c_str());

    if (m_parsingBuffer.empty())
    {
        doReadWebSocket();
        return;
    }

    struct WebsocketHeader {
        uint8_t opcode:4;
        uint8_t reserved:3;
        uint8_t fin:1;
        uint8_t payloadLength:7;
        uint8_t mask:1;
    };

    if (m_parsingBuffer.size() < sizeof(WebsocketHeader))
    {
        ESP_LOGW(TAG, "buffer smaller than a websocket header");
        doReadWebSocket();
        return;
    }

    ESP_LOGI(TAG, "%s%s%s%s %s%s%s%s    %s%s%s%s %s%s%s%s",
                  m_parsingBuffer.data()[0]&128?"1":".", m_parsingBuffer.data()[0]&64?"1":".", m_parsingBuffer.data()[0]&32?"1":".", m_parsingBuffer.data()[0]&16?"1":".",
                  m_parsingBuffer.data()[0]&8?"1":".", m_parsingBuffer.data()[0]&4?"1":".", m_parsingBuffer.data()[0]&2?"1":".", m_parsingBuffer.data()[0]&1?"1":".",
                  m_parsingBuffer.data()[1]&128?"1":".", m_parsingBuffer.data()[1]&64?"1":".", m_parsingBuffer.data()[1]&32?"1":".", m_parsingBuffer.data()[1]&16?"1":".",
                  m_parsingBuffer.data()[1]&8?"1":".", m_parsingBuffer.data()[1]&4?"1":".", m_parsingBuffer.data()[1]&2?"1":".", m_parsingBuffer.data()[1]&1?"1":".");

    const WebsocketHeader *hdr = (const WebsocketHeader *)m_parsingBuffer.data();

    ESP_LOGI(TAG, "fin=%i reserved=%i opcode=%i mask=%i payloadLength=%i", hdr->fin, hdr->reserved, hdr->opcode, hdr->mask, hdr->payloadLength);

    if (hdr->mask)
    {
        uint32_t mask;

        if (m_parsingBuffer.size() < sizeof(WebsocketHeader) + sizeof(mask) + hdr->payloadLength)
        {
            ESP_LOGW(TAG, "buffer smaller than payload %zd vs %zd", m_parsingBuffer.size(), sizeof(WebsocketHeader) + hdr->payloadLength);
            doReadWebSocket();
            return;
        }

        mask = *(const uint32_t *)(m_parsingBuffer.data() + sizeof(WebsocketHeader));
        ESP_LOGI(TAG, "mask=%s", cpputils::toHexString({(const char *)&mask, sizeof(mask)}).c_str());

        m_parsingBuffer.erase(std::begin(m_parsingBuffer), std::next(std::begin(m_parsingBuffer), sizeof(WebsocketHeader) + sizeof(mask) + hdr->payloadLength));
    }
    else
    {
        if (m_parsingBuffer.size() < sizeof(WebsocketHeader) + hdr->payloadLength)
        {
            ESP_LOGW(TAG, "buffer smaller than payload %zd vs %zd", m_parsingBuffer.size(), sizeof(WebsocketHeader) + hdr->payloadLength);
            doReadWebSocket();
            return;
        }

        m_parsingBuffer.erase(std::begin(m_parsingBuffer), std::next(std::begin(m_parsingBuffer), sizeof(WebsocketHeader) + hdr->payloadLength));
    }

    goto again;
}
