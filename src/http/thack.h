#ifndef SRC_HTTP_THACK
#define SRC_HTTP_THACK
#include "hmemory.h"
#include "httpclient.h"
#include "https/ssl_socket_wrap.h"
#include "socket_wrap.h"
#include "asio_coroutine_net.h"

#include <asio/ip/tcp.hpp>
#include <asio/use_awaitable.hpp>

#include <memory>

namespace hcpp
{
    class mem_move
    {
    public:
        virtual ~mem_move() = default;
        virtual  void make(std::unique_ptr<tcp_socket> sock)=0;
        virtual  void make(std::unique_ptr<ssl_socket> sock)=0;
    };
} // namespace hcpp

#endif /* SRC_HTTP_THACK */
