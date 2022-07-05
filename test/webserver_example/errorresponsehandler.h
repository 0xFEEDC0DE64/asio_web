#pragma once

// system includes
#include <string_view>
#include <string>
#include <system_error>

// 3rdparty lib includes
#include <asio_web/responsehandler.h>

// forward declarations
class ClientConnection;

class ErrorResponseHandler final : public ResponseHandler
{
public:
    ErrorResponseHandler(ClientConnection &clientConnection, std::string_view path);
    ~ErrorResponseHandler() final;

    void requestHeaderReceived(std::string_view key, std::string_view value) final;
    void requestBodyReceived(std::string_view body) final;
    void sendResponse() final;

private:
    void written(std::error_code ec, std::size_t length);

    ClientConnection &m_clientConnection;
    std::string m_path;

    std::string m_response;
};
