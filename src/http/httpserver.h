#ifndef HTTP_HTTP_SERVER_H
#define HTTP_HTTP_SERVER_H
#include "endpointbase.h"
#include "parser.h"
#include "socket_wrap.h"

#include <optional>

namespace hcpp
{

    struct http_response;

    struct msg_body
    {
        awaitable<bool> parser_msg_body(std::shared_ptr<memory> m, http_response &);
    };

    struct response_headers
    {
        awaitable<std::optional<msg_body>> parser_headers(std::shared_ptr<memory> m, http_response &);
    };

    struct response_line
    {
        awaitable<std::optional<response_headers>> parser_response_line(std::shared_ptr<memory> m, http_response &);
    };

    struct http_response
    {
        using http_msg_line = std::string;
        using http_msg_body = std::vector<std::string>;
        using http_headers = std::unordered_map<std::string, http_msg_line>;

        http_headers headers_;
        http_msg_body bodys_;

        response_line get_first_parser();
    };

    class http_server : public endpoint_base
    {
    public:
        awaitable<void> wait();

    public:
        http_server(tcp_socket &&sock) : endpoint_base(std::make_shared<socket_memory>(std::move(sock))) {}
    };

} // namespace hcpp

#endif