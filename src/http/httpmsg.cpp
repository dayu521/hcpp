#include "httpmsg.h"

#include <regex>
#include <cctype>

#include <spdlog/spdlog.h>

namespace hcpp
{
    // https://www.rfc-editor.org/rfc/rfc2616#section-4.2
    bool parser_header(string_view svl, http_headers &h)
    {
        using std::string;
        // Host: server.example.com
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
            // normalize成一般形式,易于比较
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
        return true;
    L:
        return false;
    }

    std::string to_lower(std::string_view s)
    {
        std::string r;
        r.reserve(s.size());
        for (auto i : s)
        {
            r += std::tolower(i);
        }
        return r;
    }

    // TODO 根据规范里的顺序进行读取.其中content-length是在Chunked之后
    std::optional<std::size_t> msg_body_size(const http_headers &header)
    {
        if (header.contains("transfer-encoding"))
        {
            // TODO Chunked
            spdlog::error("未实现 Chunked");
            return std::nullopt;
        }
        else if (header.contains("content-length"))
        {
            std::string cl = header.at("content-length");
            static std::regex digit(R"(0|[1-9]\d{0,10})");
            std::smatch m;
            if (!std::regex_search(cl, m, digit))
            {
                spdlog::info("Content-Length字段不合法 {}", cl);
                return std::nullopt;
            }

            return std::stoul(m[0].str());
        }
        return std::nullopt;
    }
    awaitable<void> http_msg::transfer_msg_body(std::shared_ptr<hcpp::memory> self, std::shared_ptr<hcpp::memory> to)
    {
        if (body_size_ > 0)
        {
            co_await transfer_mem(self, to, body_size_);
        }
    }
} // namespace hcpp
