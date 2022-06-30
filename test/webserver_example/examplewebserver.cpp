#include "examplewebserver.h"

// esp-idf includes
#include <esp_log.h>

// local includes
#include "rootresponsehandler.h"
#include "debugresponsehandler.h"
#include "chunkedresponsehandler.h"
#include "errorresponsehandler.h"
#include "websocketresponsehandler.h"

namespace {
constexpr const char * const TAG = "ASIO_WEBSERVER";
} // namespace

std::unique_ptr<ResponseHandler> ExampleWebserver::makeResponseHandler(ClientConnection &clientConnection, std::string_view method, std::string_view path, std::string_view protocol)
{
    ESP_LOGI(TAG, "method=\"%.*s\" path=\"%.*s\" protocol=\"%.*s\"",
             method.size(), method.data(), path.size(), path.data(), protocol.size(), protocol.data());
    const std::string_view processedPath{[&](){
        const auto index = path.find('?');
        return index == std::string_view::npos ?
                   path : path.substr(0, index);
    }()};

    if (processedPath.empty() || processedPath == "/")
        return std::make_unique<RootResponseHandler>(clientConnection);
    else if (processedPath == "/debug" || processedPath.starts_with("/debug/"))
        return std::make_unique<DebugResponseHandler>(clientConnection, method, path, protocol);
    else if (processedPath == "/chunked")
        return std::make_unique<ChunkedResponseHandler>(clientConnection);
    else if (processedPath == "/ws")
        return std::make_unique<WebsocketResponseHandler>(clientConnection);
    else
        return std::make_unique<ErrorResponseHandler>(clientConnection, path);
}
