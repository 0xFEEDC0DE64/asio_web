#pragma once

#pragma once

// system includes
#include <string_view>
#include <string>
#include <system_error>

// 3rdparty lib includes
#include <asio_webserver/responsehandler.h>

// forward declarations
class ClientConnection;

class WebsocketBackendResponseHandler final : public ResponseHandler
{
public:
    WebsocketBackendResponseHandler(ClientConnection &clientConnection);
    ~WebsocketBackendResponseHandler() override;

    void requestHeaderReceived(std::string_view key, std::string_view value) final;
    void requestBodyReceived(std::string_view body) final;
    void sendResponse() final;

private:
    void writtenError(std::error_code ec, std::size_t length);
    void written(std::error_code ec, std::size_t length);

    ClientConnection &m_clientConnection;

    std::string m_response;

    bool m_connectionUpgrade{};
    bool m_upgradeWebsocket{};
    std::string m_secWebsocketVersion;
    std::string m_secWebsocketKey;
    std::string m_secWebsocketExtensions;
};
