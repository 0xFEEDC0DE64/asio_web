#include "clientconnection.h"

// system includes
#include <cstdio>
#include <utility>

// esp-idf includes
#include <esp_log.h>

// local includes
#include "webserver.h"
#include "responsehandler.h"

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";
} // namespace

ClientConnection::ClientConnection(Webserver &webserver, asio::ip::tcp::socket socket) :
    m_webserver{webserver},
    m_socket{std::move(socket)},
    m_remote_endpoint{m_socket.remote_endpoint()}
{
    ESP_LOGI(TAG, "new client (%s:%hi)",
             m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());

    m_webserver.m_clients++;
}

ClientConnection::~ClientConnection()
{
    ESP_LOGI(TAG, "client destroyed (%s:%hi)",
             m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());

    m_webserver.m_clients--;
}

void ClientConnection::start()
{
    doRead();
}

void ClientConnection::responseFinished(std::error_code ec)
{
    if (ec)
    {
        ESP_LOGW(TAG, "error: %i (%s:%hi)", ec.value(),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        m_socket.close();
        return;
    }

    if constexpr (true) // Connection: Keep
    {
//        ESP_LOGD(TAG, "state changed to RequestLine");
        m_state = State::RequestLine;

        doRead();
    }
    else
        m_socket.close();
}

void ClientConnection::doRead()
{
    m_socket.async_read_some(asio::buffer(m_receiveBuffer, max_length),
                             [this, self=shared_from_this()](std::error_code ec, std::size_t length)
                             { readyRead(ec, length); });
}

void ClientConnection::readyRead(std::error_code ec, std::size_t length)
{
    if (ec)
    {
        ESP_LOGI(TAG, "error: %i (%s:%hi)", ec.value(),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        return;
    }

//    ESP_LOGV(TAG, "received: %zd \"%.*s\"", length, length, m_data);
    m_parsingBuffer.append(m_receiveBuffer, length);

    bool shouldDoRead{true};

    while (true)
    {
        constexpr std::string_view newLine{"\r\n"};
        const auto index = m_parsingBuffer.find(newLine.data(), 0, newLine.size());
        if (index == std::string::npos)
            break;

        std::string_view line{m_parsingBuffer.data(), index};

//        ESP_LOGD(TAG, "line: %zd \"%.*s\"", line.size(), line.size(), line.data());

        if (!readyReadLine(line))
            shouldDoRead = false;

        m_parsingBuffer.erase(std::begin(m_parsingBuffer), std::next(std::begin(m_parsingBuffer), line.size() + newLine.size()));
    }

    if (shouldDoRead)
        doRead();
}

bool ClientConnection::readyReadLine(std::string_view line)
{
    switch (m_state)
    {
    case State::RequestLine:
//        ESP_LOGV(TAG, "case State::RequestLine:");
        return parseRequestLine(line);
    case State::RequestHeaders:
//        ESP_LOGV(TAG, "case State::RequestHeaders:");
        return parseRequestHeader(line);
    case State::RequestBody:
//        ESP_LOGV(TAG, "case State::RequestBody:");
        ESP_LOGW(TAG, "unexpected state=RequestBody (%s:%hi)",
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        return true;
    case State::Response:
//        ESP_LOGV(TAG, "case State::Response:");
        ESP_LOGW(TAG, "unexpected state=Response (%s:%hi)",
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        return true;
    default:
        ESP_LOGW(TAG, "unknown state %i (%s:%hi)", std::to_underlying(m_state),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        return true;
    }
}

bool ClientConnection::parseRequestLine(std::string_view line)
{
    if (const auto index = line.find(' '); index == std::string::npos)
    {
        ESP_LOGW(TAG, "invalid request line (1): \"%.*s\" (%s:%hi)", line.size(), line.data(),
                 m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
        m_socket.close();
        return false;
    }
    else
    {
        const std::string_view method { line.data(), index };
//        ESP_LOGV(TAG, "request method: %zd \"%.*s\"", method.size(), method.size(), method.data());

        if (const auto index2 = line.find(' ', index + 1); index2 == std::string::npos)
        {
            ESP_LOGW(TAG, "invalid request line (2): \"%.*s\" (%s:%hi)", line.size(), line.data(),
                     m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
            m_socket.close();
            return false;
        }
        else
        {
            const std::string_view path { line.data() + index + 1, line.data() + index2 };
//            ESP_LOGV(TAG, "request path: %zd \"%.*s\"", path.size(), path.size(), path.data());

            const std::string_view protocol { line.cbegin() + index2 + 1, line.cend() };
//            ESP_LOGV(TAG, "request protocol: %zd \"%.*s\"", protocol.size(), protocol.size(), protocol.data());

            m_responseHandler = m_webserver.makeResponseHandler(*this, method, path, protocol);
            if (!m_responseHandler)
            {
                ESP_LOGW(TAG, "invalid response handler method=\"%.*s\" path=\"%.*s\" protocol=\"%.*s\" (%s:%hi)",
                         method.size(), method.data(), path.size(), path.data(), protocol.size(), protocol.data(),
                         m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
                m_socket.close();
                return false;
            }

//            ESP_LOGV(TAG, "state changed to RequestHeaders");
            m_state = State::RequestHeaders;

            return true;
        }
    }
}

bool ClientConnection::parseRequestHeader(std::string_view line)
{
    if (!line.empty())
    {
        constexpr std::string_view sep{": "};
        if (const auto index = line.find(sep.data(), 0, sep.size()); index == std::string_view::npos)
        {
            ESP_LOGW(TAG, "invalid request header: %zd \"%.*s\" (%s:%hi)", line.size(), line.size(), line.data(),
                     m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
            m_socket.close();
            return false;
        }
        else
        {
            std::string_view key{line.data(), index};
            std::string_view value{std::begin(line) + index + sep.size(), std::end(line)};

//            ESP_LOGD(TAG, "header key=\"%.*s\" value=\"%.*s\"", key.size(), key.data(), value.size(), value.data());

            if (!m_responseHandler)
            {
                ESP_LOGW(TAG, "invalid response handler (%s:%hi)",
                         m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
                m_socket.close();
                return false;
            }

            m_responseHandler->requestHeaderReceived(key, value);
            return true;
        }
    }
    else
    {
//        ESP_LOGV(TAG, "state changed to Response");
        m_state = State::Response;

        if (!m_responseHandler)
        {
            ESP_LOGW(TAG, "invalid response handler ESP_LOGI",
                     m_remote_endpoint.address().to_string().c_str(), m_remote_endpoint.port());
            m_socket.close();
            return false;
        }

        m_responseHandler->sendResponse();

        return false;
    }
}
