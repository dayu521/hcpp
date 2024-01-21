#ifndef SRC_PROXY
#define SRC_PROXY

#include "https/server.h"
#include "http/httpclient.h"
#include "http/httpserver.h"

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>

namespace hcpp
{

    using asio::awaitable;

    awaitable<void> http_proxy(http_client client, std::shared_ptr<socket_channel> https_channel);

} // namespace hcpp

#endif /* SRC_PROXY */
