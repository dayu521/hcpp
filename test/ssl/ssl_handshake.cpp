#include <asio/ssl/stream.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <print>

using namespace asio;
using tcp_socket_co = use_awaitable_t<>::as_default_on_t<ip::tcp::socket>;
using ssl_stream_co=ssl::stream<tcp_socket_co>;

// namespace log = spdlog;

int main(int argc,char ** argv){

    if(argc!=2){
        std::println("Usage: {} <hostname>", argv[0]);
        return 1;
    }

    asio::io_context io_context;

    asio::ssl::context ctx{asio::ssl::context::tlsv12};
    ctx.set_verify_mode(ssl::verify_peer);
    ctx.set_default_verify_paths();

    ssl_stream_co ssl_socket{io_context, ctx};

    
    return 0;
}