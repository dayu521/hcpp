#ifndef SRC_HTTP_HTTP
#define SRC_HTTP_HTTP

namespace hcpp
{
    struct http_request;
    struct http_response;

    using http_msg_line = std::string;
    using http_msg_body = std::vector<std::string>;
    using http_headers = std::unordered_map<std::string, http_msg_line>;

    struct msg_body
    {
        awaitable<bool> parser_msg_body(std::shared_ptr<memory> m, http_request &);
        awaitable<bool> parser_msg_body(std::shared_ptr<memory> m, http_response &);
    };

} // namespace hcpp

#endif /* SRC_HTTP_HTTP */
