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
        virtual awaitable<std::string_view> async_load_some(std::size_t max_n = 1024 * 4) {co_return "";}
        virtual awaitable<std::size_t> async_write_some(std::string_view) {co_return 0;}

        // XXX 读直到.返回已读到的字符串.
        virtual awaitable<std::string_view> async_load_until(const std::string &) {co_return "";}
        virtual awaitable<void> async_write_all(std::string_view) {co_return;}
        virtual std::string_view get_some() {return {};}
        virtual void remove_some(std::size_t index) {}
        virtual bool ok() { return read_ok_ && write_ok_; }
        virtual void reset() {}

    public:
        virtual ~memory() = default;

    protected:
        bool read_ok_=true;
        bool write_ok_=true;
    };

    inline awaitable<std::size_t> transfer_mem(std::shared_ptr<hcpp::memory> from, std::shared_ptr<hcpp::memory> to, std::size_t total_bytes)
    {
        auto nx = 0;
        auto som_str = from->get_some();
        if (!som_str.empty())
        {
            co_await to->async_write_all(som_str);
            from->remove_some(som_str.size());
            nx += som_str.size();
        }
        while (nx < total_bytes)
        {
            auto str = co_await from->async_load_some();
            co_await to->async_write_all(str);
            from->remove_some(str.size());
            nx += str.size();
        }
        co_return nx;
    }
} // namespace hcpp

#endif /* SRC_HTTP_MEMORY */
