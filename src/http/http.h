#ifndef SRC_HTTP_HTTP
#define SRC_HTTP_HTTP
#include "socket_wrap.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>

#include <asio/awaitable.hpp>

namespace hcpp
{

    struct http_request;
    struct http_response;

    using http_msg_line = std::string;
    using http_msg_body = std::vector<std::string>;
    using http_headers = std::unordered_map<std::string, http_msg_line>;

    struct http_msg
    {
        http_headers headers_;
        std::size_t body_size_ = 0;
        http_msg_body bodys_;
    };

    struct msg_body
    {
        awaitable<bool> parser_msg_body(http_request &);
        awaitable<bool> parser_msg_body(std::shared_ptr<memory> m, http_response &);
    };

} // namespace hcpp

#endif /* SRC_HTTP_HTTP */
