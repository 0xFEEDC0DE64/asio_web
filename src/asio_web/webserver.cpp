#include "webserver.h"

// esp-idf includes
#include <esp_log.h>

// local includes
#include "clientconnection.h"

namespace {
constexpr const char * const TAG = "ASIO_WEB";
} // namespace

Webserver::Webserver(asio::io_context &io_context, unsigned short port) :
    m_acceptor{io_context, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), port}}
{
    ESP_LOGI(TAG, "create webserver on port %hi", port);

    doAccept();
}

void Webserver::doAccept()
{
    m_acceptor.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket)
        { acceptClient(ec, std::move(socket)); });
}

void Webserver::acceptClient(std::error_code ec, asio::ip::tcp::socket socket)
{
    if (ec)
    {
        ESP_LOGI(TAG, "error: %i", ec.value());
        doAccept();
        return;
    }

    std::make_shared<ClientConnection>(*this, std::move(socket))->start();

    doAccept();
}
