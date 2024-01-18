#ifndef HTTP_CLIENT_HTTP_CLIENT_H
#define HTTP_CLIENT_HTTP_CLIENT_H
#include "endpointbase.h"
#include "parser.h"
#include "socket_wrap.h"

#include <optional>

namespace hcpp
{

    struct http_request;

    struct msg_body
    {
        awaitable<bool> parser_msg_body(std::shared_ptr<memory> m, http_request &);
    };

    struct request_headers
    {
        awaitable<std::optional<msg_body>> parser_headers(std::shared_ptr<memory> m, http_request &);
    };

    struct request_line
    {
        awaitable<std::optional<request_headers>> parser_reuqest_line(std::shared_ptr<memory> m, http_request &);
    };

    struct http_request
    {
        enum method
        {
            GET,
            POST,
            PUT,
            DELETE,
            HEAD,
            CONNECT,
            OPTIONS,
            TRACE,
            PATCH,
            UNKNOWN
        };

        using http_msg_line = std::string;
        using http_msg_body = std::vector<std::string>;
        using http_headers = std::unordered_map<std::string, http_msg_line>;

        method method_;
        std::string host_;
        std::string url_;
        std::string port_;
        http_headers headers_;
        http_msg_body bodys_;

        request_line get_first_parser();
    };

    class http_client : public endpoint_base
    {
    public:
        awaitable<void> wait();

    public:
        http_client(tcp_socket &&sock) : endpoint_base(std::make_shared<socket_memory>(std::move(sock))) {}
    };

} // namespace hcpp

#endif