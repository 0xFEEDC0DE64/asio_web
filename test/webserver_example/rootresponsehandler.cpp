#include "rootresponsehandler.h"

// esp-idf includes
#include <asio.hpp>
#include <esp_log.h>

// 3rdparty lib includes
#include <fmt/core.h>
#include <asio_webserver/clientconnection.h>

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";
} // namespace

RootResponseHandler::RootResponseHandler(ClientConnection &clientConnection) :
    m_clientConnection{clientConnection}
{
    ESP_LOGI(TAG, "constructed for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

RootResponseHandler::~RootResponseHandler()
{
    ESP_LOGI(TAG, "destructed for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

void RootResponseHandler::requestHeaderReceived(std::string_view key, std::string_view value)
{
}

void RootResponseHandler::requestBodyReceived(std::string_view body)
{
}

void RootResponseHandler::sendResponse()
{
    ESP_LOGI(TAG, "sending response for (%s:%hi)",
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

    m_response = fmt::format("<html>"
                                 "<head>"
                                     "<title>asio test webserver</title>"
                                 "</head>"
                                 "<body>"
                                     "<h1>asio test webserver</h1>"
                                     "<ul>"
                                         "<li><a href=\"/debug\">Debug</a></li>"
                                         "<li><a href=\"/chunked\">Chunked</a></li>"
                                     "</ul>"
                                 "</body>"
                             "</html>");

    m_response = fmt::format("HTTP/1.1 200 Ok\r\n"
                             "Connection: close\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: {}\r\n"
                             "\r\n"
                             "{}", m_response.size(), m_response);

    asio::async_write(m_clientConnection.socket(),
                      asio::buffer(m_response.data(), m_response.size()),
                      [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                      { written(ec, length); });
}

void RootResponseHandler::written(std::error_code ec, std::size_t length)
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
