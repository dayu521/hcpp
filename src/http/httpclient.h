#ifndef SRC_HTTP_HTTPCLIENT
#define SRC_HTTP_HTTPCLIENT
#include "memory.h"
#include "socket_wrap.h"
#include "httpmsg.h"
#include "asio_coroutine_net.h"

#include <optional>

namespace hcpp
{
    struct request_headers
    {
        std::size_t header_end_;
        std::shared_ptr<memory> m_;
        awaitable<std::optional<msg_body>> parser_headers(http_request &);
    };

    struct request_line
    {
        std::shared_ptr<memory> m_;
        awaitable<std::optional<request_headers>> parser_reuqest_line(http_request &);
    };

    struct http_request : http_msg
    {
        enum method
        {
            CONNECT,
            OTHER
        };

        method method_;
        std::string version_;
        std::string method_str_;
        std::string host_;
        std::string url_;
        std::size_t url_param_start_;
        std::string port_;

        request_line get_first_parser(std::shared_ptr<memory> m);
    };

    class http_client
    {
    public:
        http_client(tcp_socket &&sock);
        http_client() = default;
        virtual ~http_client() = default;

        std::shared_ptr<memory> get_memory() { return mem_; }

        virtual http_request make_request() const;

        virtual awaitable<void> init(){co_return;}

    protected:
        std::shared_ptr<memory> mem_;
    };

} // namespace hcpp

#endif /* SRC_HTTP_HTTPCLIENT */
