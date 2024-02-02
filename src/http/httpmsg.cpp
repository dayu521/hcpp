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

    inline std::size_t hex_str_to(const string_view &hex_str_v)
    {
        std::smatch m;
        std::string hex_str{hex_str_v.data(), hex_str_v.size()};
        if (!std::regex_search(hex_str, m, hex_reg))
        {
            log::error("hex_str不合法 {}", hex_str.substr(0, 30));
            for (auto i : hex_str.substr(0, 30))
            {
                log::error("{}", (unsigned int)(i & 255));
            }
            throw std::runtime_error("hex_str正则表达式匹配失败");
        }
        return std::stoul(m.str(), nullptr, 16);
    }

    // https://www.rfc-editor.org/rfc/rfc2616#section-3.6.1
    awaitable<std::size_t> http_msg::transfer_chunk(std::shared_ptr<hcpp::memory> self, std::shared_ptr<hcpp::memory> to)
    {
        std::size_t n = 0;
        self->merge_some();

        //  缓存里可能包含多个块,我们找到最后一块
        //  这样,这一块跨越了缓存(小概率正好不在缓存中)
        //  那么处理这一块的传输后,所有的后续传输都不用处理缓存与块相交织的情况了

        // buff中可能的数据
        // ||chunk_size|\r\n|chunk|\r\n
        // ||chunk_size...

        while (true)
        {
            // chunk_size|\r\n 最大不超过18个字符
            auto some_str = self->get_some();
            std::string cs_str = some_str.substr(0, 18).data();
            auto p = cs_str.find("\r\n");
            while (p == std::string::npos && cs_str.size() < 18)
            {
                some_str = co_await self->async_load_some();
                cs_str += some_str.substr(0, 18 - cs_str.size());
                p = cs_str.find("\r\n");
            }
            if (p == std::string::npos)
            {
                log::error("无法解析chunk大小");
                for (auto &&i : cs_str)
                {
                    log::error("{} ", static_cast<unsigned int>(i));
                }
                goto F;
            }

            cs_str.resize(p);

            auto chunk_size = hex_str_to(cs_str);
            if (chunk_size == 0)
            {
                break;
            }
            auto complete_chunk_size = p + 2 + chunk_size + 2;
            if (auto tn = co_await transfer_mem(self, to, complete_chunk_size); tn != complete_chunk_size)
            {
                n += tn;
                log::error("chunk大小与实际大小不符 {} {}", complete_chunk_size, tn);
                goto F;
            }
            n += complete_chunk_size;
        }

        if (n == 0)
        {
            log::error("没有解析到chunk");
            goto F;
        }

        // 传送剩下的.通常剩下的字节数并不多.
        n += co_await transfer_mem_until(self, to, "\r\n\r\n");
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
