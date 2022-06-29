#include "examplewebserver.h"

#include "rootresponsehandler.h"
#include "debugresponsehandler.h"
#include "errorresponsehandler.h"

std::unique_ptr<ResponseHandler> ExampleWebserver::makeResponseHandler(ClientConnection &clientConnection, std::string_view method, std::string_view path, std::string_view protocol)
{
    const std::string_view processedPath{[&](){
        const auto index = path.find('?');
        return index == std::string_view::npos ?
                   path : path.substr(0, index);
    }()};

    if (processedPath.empty() || processedPath == "/")
        return std::make_unique<RootResponseHandler>(clientConnection);
    else if (processedPath == "/debug" || processedPath.starts_with("/debug/") )
        return std::make_unique<DebugResponseHandler>(clientConnection, method, path, protocol);
    else
        return std::make_unique<ErrorResponseHandler>(clientConnection, path);
}
