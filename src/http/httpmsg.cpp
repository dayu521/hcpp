#include "httpmsg.h"

#include <regex>
#include <cctype>

#include <spdlog/spdlog.h>

namespace hcpp
{
    namespace log = spdlog;
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

    inline std::regex hex_reg(R"(^[1-9A-Fa-f][0-9A-Fa-f]{0,15})");
    inline std::regex zero_reg(R"(^0{0,15})");

    awaitable<std::size_t> http_msg::transfer_chunk(std::shared_ptr<hcpp::memory> self, std::shared_ptr<hcpp::memory> to)
    {
        std::size_t n = 0;

        auto s = self->get_some();

        std::string tem_str;
        //BUG

        // 解析*chunk
        auto line = self->get_some();
        if (line.size() == 0)
        {
            line = co_await self->async_load_until("\r\n");
        }
        else if (auto p=line.find("\r\n");p== std::string::npos)
        {
            tem_str=line;
            if (tem_str.back() == '\r')
            {
                tem_str += co_await self->async_load_some(1);
                if (tem_str.back() != '\n')
                {
                    goto F;
                }
            }
            else
            {
                tem_str += co_await self->async_load_until("\r\n");
            }
            line=tem_str;
        }
        else
        {
            line=line.substr(0,p+2);
        }

        while (line.size() > 2)
        {
            // 解析块大小行
            std::smatch m;
            std::string line_str{line.data(), line.size() - 2};
            if (!std::regex_search(line_str, m, hex_reg))
            {
                log::error("chunk大小不合法 {}", line);
                for (auto i : line.substr(0, 30))
                {
                    log::error("{}", (unsigned int)(i & 255));
                }
                goto F;
            }
            std::size_t chunk_size = std::stoul(m.str(), nullptr, 16);
            auto total_chunk_size = chunk_size + 2 + line.size();

            // 把块大小和块一同传送
            if (auto tn = co_await transfer_mem(self, to, total_chunk_size); tn != total_chunk_size)
            {
                n += tn;
                self->remove_some(tn);
                log::error("chunk大小与实际大小不符 {} {}", total_chunk_size, tn);
                goto F;
            }
            n += total_chunk_size;
            self->remove_some(total_chunk_size);

            line = co_await self->async_load_until("\r\n");
        }

        if(n==0){
            log::error("没有解析到chunk");
            goto F;
        }

        if (line.size() == 2)
        {
            log::error("last-chunk不正确: {}", line);
            goto F;
        }

        // 解析剩下的
        line = co_await self->async_load_until("\r\n\r\n");
        co_await to->async_write_all(line);
        self->remove_some(line.size());

        n += line.size();
        co_return n;
    F:
        throw std::runtime_error("chunked transfer error");
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
        if (header.contains("content-length"))
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
