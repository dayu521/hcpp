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

        }
        return true;
    L:
        return false;
    }

    inline std::regex hex_reg(R"(^0{1,15}|[1-9A-Fa-f][0-9A-Fa-f]{0,15})");
    inline std::regex zero_reg(R"(^0{1,15})");

    // https://www.rfc-editor.org/rfc/rfc2616#section-3.6.1
    awaitable<std::size_t> http_msg::transfer_chunk(std::shared_ptr<hcpp::memory> self, std::shared_ptr<hcpp::memory> to)
    {
        std::size_t n = 0;
        self->merge_some();

        std::string tem_str;

        // 从缓存中获取chunk大小行
        auto line = self->get_some();
        if (auto p = line.find("\r\n"); p == std::string::npos)
        {
            if (line.empty())
            {
                line = co_await self->async_load_until("\r\n");
                line.remove_suffix(2);
            }
            else if (line.back() == '\r')
            {
                if ((co_await self->async_load_some()).front() == '\n')
                {
                    line.remove_suffix(1);
                }
                else
                {
                    log::error("ctlr读取错误,期待\\n");
                    goto F;
                }
            }
            else
            {
                auto next_str = co_await self->async_load_until("\r\n");
                if (next_str.size() == 2)
                {
                    ;
                }
                else
                {
                    tem_str = line;
                    tem_str += next_str;
                    line = tem_str;
                    line.remove_suffix(2);
                }
            }
        }
        else
        {
            line = line.substr(0, p);
        }

        log::error("chunk块 开始");
        while (line.size() > 0)
        {
            // 解析块大小行
            std::smatch m;
            std::string line_str{line.data(), line.size()};
            if (!std::regex_search(line_str, m, hex_reg))
            {
                log::error("chunk大小不合法 {}", line.substr(0, 30));
                for (auto i : line.substr(0, 30))
                {
                    log::error("{}", (unsigned int)(i & 255));
                }
                goto F;
            }
            std::size_t chunk_size = std::stoul(m.str(), nullptr, 16);

            if (chunk_size == 0)
            {
                break;
            }

            auto total_chunk_size = chunk_size + 2 + line.size() + 2;

            // 把块大小和块一同传送
            //BUG
            if (auto tn = co_await transfer_mem(self, to, total_chunk_size); tn != total_chunk_size)
            {
                n += tn;
                log::error("chunk大小与实际大小不符 {} {}", total_chunk_size, tn);
                goto F;
            }
            n += total_chunk_size;
            assert(self->merge_some() == 0);
            //BUG 处理异常
            log::error("读取下一块");
            line = co_await self->async_load_until("\r\n");
            line.remove_suffix(2);
        }
        log::error("chunk块 完成");

        if (n == 0)
        {
            log::error("没有解析到chunk");
            goto F;
        }

        // 传送剩下的.通常剩下的字节数并不多.
        log::error("transfer_mem_until 开始");
        n += co_await transfer_mem_until(self, to, "\r\n\r\n");
        log::error("transfer_mem_until 结束");
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

    // 根据规范里的顺序,其中content-length优先级是在Chunked之后
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
