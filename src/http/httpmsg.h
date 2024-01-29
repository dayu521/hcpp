#ifndef SRC_HTTP_HTTPBASE
#define SRC_HTTP_HTTPBASE
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
        bool chunk_coding_ = false;
        http_msg_body bodys_;
        bool has_error_ = true;

        awaitable<void> transfer_msg_body(std::shared_ptr<hcpp::memory> self, std::shared_ptr<hcpp::memory> to);

        awaitable<std::size_t> transfer_chunk(std::shared_ptr<hcpp::memory> self, std::shared_ptr<hcpp::memory> to);
    };

    struct msg_body
    {
        awaitable<bool> parser_msg_body(http_request &);
        awaitable<bool> parser_msg_body(http_response &);
    };

    bool parser_header(std::string_view svl, http_headers &h);

    

    std::string to_lower(std::string_view s);

    std::optional<std::size_t> msg_body_size(const http_headers &header);

} // namespace hcpp

#endif /* SRC_HTTP_HTTPBASE */
