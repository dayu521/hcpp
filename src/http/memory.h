#ifndef SRC_HTTP_MEMORY
#define SRC_HTTP_MEMORY
#include <cstddef>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <utility>

#include <asio/awaitable.hpp>

namespace hcpp
{
    using namespace asio;

    class memory
    {
    public:
        virtual awaitable<void> wait() = 0;

        // XXX读取最大max_n的数据,放到buff中.
        virtual awaitable<std::string_view> async_load_some(std::size_t max_n = 1024 * 4) = 0;
        virtual awaitable<std::size_t> async_write_some(std::string_view ) = 0;

        // XXX 读直到.返回已读到的字符串.
        virtual awaitable<std::string_view> async_load_until(const std::string &) = 0;
        virtual awaitable<void> async_write_all(std::string_view ) = 0;
        virtual std::string_view get_some() = 0;
        virtual void remove_some(std::size_t index) = 0;
        virtual bool ok() = 0;
        virtual void reset()=0;

    public:
        virtual ~memory() = default;
    };

} // namespace hcpp

#endif /* SRC_HTTP_MEMORY */
