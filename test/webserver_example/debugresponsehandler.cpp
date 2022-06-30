#include "debugresponsehandler.h"

// esp-idf includes
#include <asio.hpp>
#include <esp_log.h>

// 3rdparty lib includes
#include <fmt/core.h>
#include <asio_webserver/clientconnection.h>

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";
} // namespace

DebugResponseHandler::DebugResponseHandler(ClientConnection &clientConnection, std::string_view method, std::string_view path, std::string_view protocol) :
    m_clientConnection{clientConnection}, m_method{method}, m_path{path}, m_protocol{protocol}
{
    ESP_LOGI(TAG, "constructed for %.*s %.*s (%s:%hi)", m_method.size(), m_method.data(), path.size(), path.data(),
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

DebugResponseHandler::~DebugResponseHandler()
{
    ESP_LOGI(TAG, "destructed for %.*s %.*s (%s:%hi)", m_method.size(), m_method.data(), m_path.size(), m_path.data(),
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
}

void DebugResponseHandler::requestHeaderReceived(std::string_view key, std::string_view value)
{
    m_requestHeaders.emplace_back(std::make_pair(std::string{key}, std::string{value}));
}

void DebugResponseHandler::requestBodyReceived(std::string_view body)
{
    m_requestBody += body;
}

void DebugResponseHandler::sendResponse()
{
    ESP_LOGI(TAG, "sending response for %.*s %.*s (%s:%hi)", m_method.size(), m_method.data(), m_path.size(), m_path.data(),
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

    m_response = fmt::format("<html>"
                                 "<head>"
                                     "<title>Test</title>"
                                 "</head>"
                                 "<body>"
                                     "<h1>Request details:</h1>"
                                     "<table>"
                                         "<tbody>"
                                             "<tr><th>Method:</th><td>{}</td></tr>"
                                             "<tr><th>Path:</th><td>{}</td></tr>"
                                             "<tr><th>Protocol:</th><td>{}</td></tr>"
                                             "<tr>"
                                                 "<th>Request headers:</th>"
                                                 "<td>"
                                                     "<table border=\"1\">",
        m_method, m_path, m_protocol);

    for (const auto &pair : m_requestHeaders)
        m_response += fmt::format(                       "<tr><th>{}</th><td>{}</td></tr>",
            pair.first, pair.second);

    m_response +=                                    "</table>"
                                                 "</td>"
                                             "</tr>";

    if (!m_requestBody.empty())
    {
        m_response += fmt::format(           "<tr>"
                                                 "<th>Request body:</th>"
                                                 "<td><pre>{}</pre></td>"
                                             "</tr>", m_requestBody);
    }

    m_response +=                        "</tbody>"
                                     "</table>"
                                     "<form method=\"GET\">"
                                         "<fieldset>"
                                             "<legend>GET form with line edit</legend>"
                                             "<label>Text-Input: <input type=\"text\" name=\"inputName\" /></label>"
                                             "<button type=\"submit\">Go</button>"
                                         "</fieldset>"
                                     "</form>"
                                     "<form method=\"POST\">"
                                         "<fieldset>"
                                             "<legend>POST form with line edit</legend>"
                                             "<label>Text-Input: <input type=\"text\" name=\"inputName\" /></label>"
                                             "<button type=\"submit\">Go</button>"
                                         "</fieldset>"
                                     "</form>"
                                     "<form method=\"POST\">"
                                         "<fieldset>"
                                             "<legend>POST form with multiline edit</legend>"
                                             "<label>Text-Input: <textarea name=\"inputName\"></textarea></label>"
                                             "<button type=\"submit\">Go</button>"
                                         "</fieldset>"
                                     "</form>"
                                     "<form method=\"POST\" enctype=\"multipart/form-data\">"
                                         "<fieldset>"
                                             "<legend>POST form with multipart form-data and line edit</legend>"
                                             "<label>Text-Input: <input type=\"text\" name=\"inputName\" /></label>"
                                             "<button type=\"submit\">Go</button>"
                                         "</fieldset>"
                                     "</form>"
                                     "<form method=\"POST\" enctype=\"multipart/form-data\">"
                                         "<fieldset>"
                                             "<legend>POST form with multipart form-data and file upload</legend>"
                                             "<label>File-Upload: <input type=\"file\" name=\"inputName\" /></label>"
                                             "<button type=\"submit\">Go</button>"
                                         "</fieldset>"
                                     "</form>"
                                 "</body>"
                             "</html>";

    m_response = fmt::format("HTTP/1.1 200 Ok\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: {}\r\n"
                             "\r\n"
                             "{}",
                             m_response.size(), m_response);

    asio::async_write(m_clientConnection.socket(),
                      asio::buffer(m_response.data(), m_response.size()),
                      [this, self=m_clientConnection.shared_from_this()](std::error_code ec, std::size_t length)
                      { written(ec, length); });
}

void DebugResponseHandler::written(std::error_code ec, std::size_t length)
{
    if (ec)
    {
        ESP_LOGW(TAG, "error: %i (%s:%hi)", ec.value(),
                 m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());
        m_clientConnection.responseFinished(ec);
        return;
    }

    ESP_LOGI(TAG, "expected=%zd actual=%zd for %.*s %.*s (%s:%hi)", m_response.size(), length,
             m_method.size(), m_method.data(), m_path.size(), m_path.data(),
             m_clientConnection.remote_endpoint().address().to_string().c_str(), m_clientConnection.remote_endpoint().port());

    m_clientConnection.responseFinished(ec);
}
