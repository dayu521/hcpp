#include "httpclient.h"
#include "parser.h"

#include <regex>

#include <spdlog/spdlog.h>

namespace hcpp
{

    inline http_request::method get_method(std::string_view sv)
    {
        if (sv == "CONNECT")
            return http_request::method::CONNECT;
        if (sv == "GET")
            return http_request::method::GET;
        if (sv == "POST")
            return http_request::method::POST;
        if (sv == "PUT")
            return http_request::method::PUT;
        if (sv == "DELETE")
            return http_request::method::DELETE;
        if (sv == "HEAD")
            return http_request::method::HEAD;
        if (sv == "OPTIONS")
            return http_request::method::OPTIONS;
        if (sv == "TRACE")
            return http_request::method::TRACE;
        if (sv == "PATCH")
            return http_request::method::PATCH;
        
        return http_request::method::UNKNOWN;
    }

    awaitable<std::optional<request_headers>> hcpp::request_line::parser_reuqest_line(std::shared_ptr<memory> m, http_request &h)
    {
        auto msg = co_await m->async_load_until("\r\n\r\n");
        auto svl = msg.substr(0, msg.find("\r\n") + 2);
        auto rm = svl.size();

        {
            // method
            auto method_end = svl.find_first_of(" ");
            if (method_end == string_view::npos)
            {
                goto L;
            }
            h.method_ = get_method(svl.substr(0, method_end));
            svl.remove_prefix(method_end + 1);

            // host和port
            auto uri_end = svl.find_first_of(" ");
            auto service_end = uri_end;
            if (uri_end == std::string_view::npos)
            {
                goto L;
            }
            if (h.method_ != http_request::CONNECT)
            {
                constexpr auto ll = sizeof("http://") - 1;
                if (!svl.starts_with("http://"))
                {
                    goto L;
                }
                svl.remove_prefix(ll);
                uri_end -= ll;
                service_end -= ll;
                auto slash_bengin = svl.substr(0, uri_end).find_first_of('/');
                if (slash_bengin != string_view::npos)
                {
                    service_end = slash_bengin;
                }
            }
            static std::regex endpoint_reg(R"(^([\w\.\-]+)(:(0|[1-9]\d{0,4}))?)");
            std::cmatch m;
            if (!std::regex_match(svl.begin(), svl.begin() + service_end, m, endpoint_reg))
            {
                spdlog::error("匹配endpoint_reg 正则表达式失败 输入为: {}", svl.substr(0, uri_end));
                goto L;
            }
            assert(m[1].matched);
            h.host_ = m[1].str();
            if (m[3].matched)
            {
                auto port = 0;
                port = std::stoi(m[3].str());
                if (port > 65535)
                {
                    spdlog::error("端口号不合法");
                    goto L;
                }
                // 插入失败说明存在了
                h.port_ = m[3].str();
            }
            else
            {
                // 没有端口号.如果是非CONNECT,给它一个默认端口号
                h.port_ = "80";
            }
            svl.remove_prefix(service_end);

            auto url_end = uri_end - service_end;
            // url
            if (h.method_ != http_request::CONNECT)
            {
                h.url_ = svl.substr(0, url_end);
            }
            assert(url_end + 1 <= svl.size());
            svl.remove_prefix(url_end + 1);

            // 检查http协议版本
            if (!svl.starts_with("HTTP/"))
            {
                // error = "HTTP/1.1 405 Method Not Allowed";
                goto L;
            }
            svl.remove_prefix(sizeof "HTTP/" - 1);
            auto http_ver_end = svl.find("\r\n");

            if (http_ver_end == std::string_view::npos)
            {
                goto L;
            }
            auto http_ver = svl.substr(0, http_ver_end);
            static std::regex digit(R"(^\d{1,5}\.\d{1,10}$)");
            if (!std::regex_match(http_ver.begin(), http_ver.end(), digit))
            {
                spdlog::error("http 协议版本 正则表达式匹配不正确");
                goto L;
            }
            assert(http_ver_end + 1 <= svl.size());
            svl.remove_prefix(http_ver_end + 2);
            assert(svl.size() == 0);
        }
        m->remove_some(rm);
        co_return std::make_optional<request_headers>();
    L:
        co_return std::nullopt;
    }

    awaitable<std::optional<msg_body>> hcpp::request_headers::parser_headers(std::shared_ptr<memory> m, http_request &h)
    {
        auto sv = m->get_some();
        auto rm = sv.size();
        if (sv.starts_with("\r\n"))
        {
            co_return std::nullopt;
        }

        if (parser_header(sv, h.headers_))
        {
            co_return std::make_optional<msg_body>();
        }
        co_return std::nullopt;
    }

    awaitable<bool> hcpp::msg_body::parser_msg_body(std::shared_ptr<memory> m, http_request &h)
    {
        co_return false;
    }

    request_line hcpp::http_request::get_first_parser()
    {
        return {};
    }

    awaitable<void> http_client::wait()
    {
        co_await get_memory()->wait();
    }

} // namespace hcpp