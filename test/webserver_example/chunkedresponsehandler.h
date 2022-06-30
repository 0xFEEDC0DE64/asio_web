#pragma once

// system includes
#include <string_view>
#include <string>
#include <system_error>

// 3rdparty lib includes
#include <asio_webserver/responsehandler.h>

// forward declarations
class ClientConnection;

class ChunkedResponseHandler final : public ResponseHandler
{
public:
    ChunkedResponseHandler(ClientConnection &clientConnection);
    ~ChunkedResponseHandler() final;

    void requestHeaderReceived(std::string_view key, std::string_view value) final;
    void requestBodyReceived(std::string_view body) final;
    void sendResponse() final;

private:
    void written(std::error_code ec, std::size_t length);

    ClientConnection &m_clientConnection;

    std::string m_response;

    int m_counter{};
};
