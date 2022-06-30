#include "chunkedresponsehandler.h"

// esp-idf includes
#include <asio.hpp>
#include <esp_log.h>

// 3rdparty lib includes
#include <fmt/core.h>
#include <asio_webserver/clientconnection.h>

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";
} // namespace

ChunkedResponseHandler::ChunkedResponseHandler(ClientConnection &clientConnection) :
    m_clientConnection{clientConnection}
{
    ESP_LOGI(TAG, "constructed for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

ChunkedResponseHandler::~ChunkedResponseHandler()
{
    ESP_LOGI(TAG, "destructed for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

void ChunkedResponseHandler::requestHeaderReceived(std::string_view key, std::string_view value)
{
}

void ChunkedResponseHandler::sendResponse()
{
    ESP_LOGI(TAG, "sending response (header) for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

    m_response = fmt::format("HTTP/1.1 200 Ok\r\n"
                             "Connection: close\r\n"
                             "Content-Type: text/html\r\n"
                             "Transfer-Encoding: chunked\r\n"
                             "\r\n");

    asio::async_write(m_clientConnection.socket(),
                      asio::buffer(m_response.data(), m_response.size()),
                      [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                      { written(ec, length); });
}

void ChunkedResponseHandler::written(std::error_code ec, std::size_t length)
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

    if (m_counter < 10)
    {
        ESP_LOGI(TAG, "sending response (line %i) for (%s:%hi)", m_counter,
                 m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

        m_response = fmt::format("Line number {}<br/>\n", m_counter);
        m_response = fmt::format("{:x}\r\n"
                                 "{}\r\n",
                                 m_response.size(), m_response);

        asio::async_write(m_clientConnection.socket(),
                          asio::buffer(m_response.data(), m_response.size()),
                          [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                          { written(ec, length); });

        m_counter++;
    }
    else if (m_counter == 10)
    {
        ESP_LOGI(TAG, "sending response (end) for (%s:%hi)",
                 m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

        m_response = fmt::format("0\r\n"
                                 "\r\n");

        asio::async_write(m_clientConnection.socket(),
                          asio::buffer(m_response.data(), m_response.size()),
                          [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                          { written(ec, length); });

        m_counter++;
    }
    else
    {
        ESP_LOGI(TAG, "end");
        m_clientConnection.responseFinished(ec);
    }
}
