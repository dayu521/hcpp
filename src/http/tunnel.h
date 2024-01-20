#ifndef SRC_HTTP_TUNNEL2
#define SRC_HTTP_TUNNEL2

#include "httpclient.h"
#include "httpserver.h"

#include <memory>

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>

namespace hcpp
{
    using namespace asio;

    using ip::tcp;
    using default_token = use_awaitable_t<>;
    using tcp_acceptor = default_token::as_default_on_t<tcp::acceptor>;
    using tcp_socket = default_token::as_default_on_t<tcp::socket>;


    awaitable<void> make_bind(http_client client, http_server server,std::string host, std::string service );
    awaitable<void> bind_write2(std::shared_ptr<memory> client, std::shared_ptr<memory> server, std::string server_info);
    awaitable<void> bind_read2(std::shared_ptr<memory> client, std::shared_ptr<memory> server, std::string server_info);

} // namespace hcpp

#endif /* SRC_HTTP_TUNNEL */
