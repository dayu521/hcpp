#include <spdlog/spdlog.h>

#include <regex>
#include <cctype>

#include "parser.h"

inline std::regex endpoint_reg{R"(^([\w\.]+)(:(0|[1-9]\d{0,4}))?$)"};
inline std::regex valreg{R"(^[\w]$)"};

bool check_req_line_1(string_view svl, RequestSvcInfo &ts, string &req_line_new)
{
    //"CONNECT * HTTP/1.1\r\n";
    {
        auto method_end = svl.find_first_of(" ");
        if (method_end == string_view::npos)
        {
            goto L;
        }
        ts.method_ = svl.substr(0, method_end);
        assert(req_line_new.empty());
        // 包含空格
        req_line_new += svl.substr(0, method_end + 1);
        svl.remove_prefix(method_end + 1);

        auto uri_end = svl.find_first_of(" ");

        if (uri_end == std::string_view::npos)
        {
            goto L;
        }
        auto service_end = uri_end;
        if (ts.method_ != "CONNECT")
        {
            constexpr auto ll = sizeof("http://") - 1;
            if (!svl.starts_with("http://"))
            {
                goto L;
            }

            svl.remove_prefix(ll);
            uri_end -= ll;
            service_end -= ll;
            auto slash_bengin = svl.substr(0,uri_end).find_first_of('/');
            if (slash_bengin != string_view::npos)
            {
                service_end = slash_bengin;
                // 请求uri改写完成
                req_line_new += svl.substr(slash_bengin);
            }
            else
            {
                req_line_new += '/';
                req_line_new+=svl.substr(service_end);
            }
        }

        std::regex endpoint_reg(R"(^([\w\.\-]+)(:(0|[1-9]\d{0,4}))?)");
        std::cmatch m;
        if (!std::regex_match(svl.begin(), svl.begin() + service_end, m, endpoint_reg))
        {
            spdlog::error("匹配endpoint_reg 正则表达式失败 输入为: {}", svl.substr(0, service_end));
            goto L;
        }

        if (spdlog::get_level() == spdlog::level::trace)
        {
            for (int i = 0; auto &&sm : m)
            {
                spdlog::trace("    {}", sm.str());
                // std::cout << "    " << sm.str() << std::endl;
            }
        }
        assert(m[1].matched);
        ts.host_ = m[1].str();
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
            ts.port_ = m[3].str();
        }
        else
        {
            // 没有端口号.如果是非CONNECT,给它一个默认端口号
            ts.port_ = "80";
        }

        // 连同" "一起跳过
        assert(uri_end + 1 <= svl.size());
        svl.remove_prefix(uri_end + 1);

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
    return true;
L:
    return false;
}

// https://www.rfc-editor.org/rfc/rfc2616#section-4.2
bool parser_header(string_view svl, headers &h)
{
    // Host: server.example.com
    bool ok = false;
    string_view error;
    while (svl.size() > 0)
    {
        auto svl_old = svl;
        auto name_end = svl.find(": ");
        if (name_end == string_view::npos)
        {
            goto L;
        }
        string name{svl.substr(0, name_end)};
        svl.remove_prefix(name_end + 2);

        // 找到所有值
        auto val_end = svl.find("\r\n");
        if (val_end == string_view::npos)
        {
            goto L;
        }
        svl_old = svl_old.substr(0, name_end + 2 + val_end + 2);
        //normalize成一般形式,易于比较
        h[to_lower(name)] = svl_old;

        string val;
        val += svl.substr(0, val_end);
        svl.remove_prefix(val_end + 2);
        // 如果可能的结束点跟随" ",则它不是真正的结束点
        while (svl.starts_with(" "))
        {
            val += " ";
            svl.remove_prefix(1);
            val_end = svl.find("\r\n");
            if (name_end == string_view::npos)
            {
                goto L;
            }
            val += svl.substr(0, val_end);
            svl.remove_prefix(val_end + 2);
        }
        // // 我们认为val中的多值都会是空白分割的,注意,分割空白之后的空白都是空白的值
        // if (auto [it, ok] = h.emplace(name, val); !ok)
        // {
        //     h[name] += " ";
        //     h[name] += val;
        // }
    }
T:
    return true;
L:
    return false;
}

string to_lower(string_view s)
{
    string r;
    r.reserve(s.size());
    for (auto i : s)
    {
        r += std::tolower(i);
    }
    return r;
}
