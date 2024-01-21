#ifndef SRC_HTTP_HTTPCLIENT
#define SRC_HTTP_HTTPCLIENT
#include "memory.h"
#include "parser.h"
#include "socket_wrap.h"
#include "http.h"

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
        awaitable<std::optional<request_headers>> parser_reuqest_line( http_request &);
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
        std::string port_;
        http_headers headers_;

        request_line get_first_parser(std::shared_ptr<memory> m);
    };

    class http_client
    {

    public:
        http_client(tcp_socket &&sock);

        std::shared_ptr<memory> get_memory(){return mem_;}

    private:
        std::shared_ptr<socket_memory> mem_;
    };

} // namespace hcpp

#endif /* SRC_HTTP_HTTPCLIENT */
