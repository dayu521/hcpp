#ifndef HTTP_CLIENT_HTTP_CLIENT_H
#define HTTP_CLIENT_HTTP_CLIENT_H
#include "clientbase.h"
#include "parser.h"

namespace hcpp
{
    using http_client = client_base;

    template <>
    awaitable<void> read(std::shared_ptr<memory> m, http_request::reuqest_line_ &line)
    {
        line = co_await m->async_read_until("\r\n");
    }
    template <>
    awaitable<void> read(std::shared_ptr<memory> m, http_request::http_headers &headers)
    {
        auto header_str = co_await m->async_read_until("\r\n");
        if(header_str.size()==2){
            co_return;
        }
        parerr_header(header_str, headers);
    }
    template <>
    awaitable<void> read(std::shared_ptr<memory> m, http_request::http_msg_body &line)
    {
    }

} // namespace hcpp

#endif