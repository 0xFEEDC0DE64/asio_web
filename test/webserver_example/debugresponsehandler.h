#pragma once

// system includes
#include <string_view>
#include <string>
#include <utility>
#include <vector>
#include <system_error>

// 3rdparty lib includes
#include <asio_webserver/responsehandler.h>

// forward declarations
class ClientConnection;

class DebugResponseHandler final : public ResponseHandler
{
public:
    DebugResponseHandler(ClientConnection &clientConnection, std::string_view method, std::string_view path, std::string_view protocol);
    ~DebugResponseHandler() final;

    void requestHeaderReceived(std::string_view key, std::string_view value) final;
    void requestBodyReceived(std::string_view body) final;
    void sendResponse() final;

private:
    void written(std::error_code ec, std::size_t length);

    ClientConnection &m_clientConnection;
    std::string m_method;
    std::string m_path;
    std::string m_protocol;

    std::vector<std::pair<std::string, std::string>> m_requestHeaders;

    std::string m_requestBody;

    std::string m_response;
};
