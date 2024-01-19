#ifndef SRC_HTTP_HTTPCLIENT
#define SRC_HTTP_HTTPCLIENT
#include "memory.h"
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
            CONNECT,
            OTHER
        };

        using http_msg_line = std::string;
        using http_msg_body = std::vector<std::string>;
        using http_headers = std::unordered_map<std::string, http_msg_line>;

        method method_;
        std::string version_;
        std::string method_str_;
        std::string host_;
        std::string url_;
        std::string port_;
        http_headers headers_;
        http_msg_body bodys_;

        request_line get_first_parser();
    };

    class http_client
    {

    public:
        http_client(tcp_socket &&sock);

        std::shared_ptr<memory> get_memory(){return mem_;}

    private:
        std::shared_ptr<memory> mem_;
    };

} // namespace hcpp

#endif /* SRC_HTTP_HTTPCLIENT */
