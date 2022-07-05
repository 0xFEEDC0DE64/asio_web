#pragma once

#pragma once

// system includes
#include <string_view>
#include <string>
#include <system_error>

// 3rdparty lib includes
#include <asio_web/responsehandler.h>

// forward declarations
class ClientConnection;

class WebsocketResponseHandler final : public ResponseHandler
{
public:
    WebsocketResponseHandler(ClientConnection &clientConnection);
    ~WebsocketResponseHandler() override;

    void requestHeaderReceived(std::string_view key, std::string_view value) final;
    void requestBodyReceived(std::string_view body) final;
    void sendResponse() final;

private:
    void writtenHtmlHeader(std::error_code ec, std::size_t length);
    void writtenHtml(std::error_code ec, std::size_t length);
    void writtenWebsocket(std::error_code ec, std::size_t length);

    ClientConnection &m_clientConnection;

    std::string m_response;

    bool m_connectionUpgrade{};
    bool m_upgradeWebsocket{};
    std::string m_secWebsocketVersion;
    std::string m_secWebsocketKey;
    std::string m_secWebsocketExtensions;
};
