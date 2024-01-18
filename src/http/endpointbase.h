#ifndef HTTP_CLIENT_CLIENT_BASE_H
#define HTTP_CLIENT_CLIENT_BASE_H
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

        virtual awaitable<void> wait()=0;

        // XXX读取最大max_n的数据,放到buff中.可以提供数据存放在buff的开始点
        virtual awaitable<std::string_view> async_load_some(std::size_t max_n=1024*4) = 0;
        virtual awaitable<std::size_t> async_write_some(const std::string &) = 0;

        // XXX 读直到.返回已读到的字符串,和剩下在缓存中的数量
        virtual awaitable<std::string_view> async_load_until(const std::string &) = 0;
        virtual awaitable<void> async_write_all(const std::string &) = 0;
        virtual std::string_view get_some()=0;
        virtual void remove_some(std::size_t index)=0;
        virtual bool eof() = 0;

    public:
        virtual ~memory() = default;
    };

    class endpoint_base
    {
    public:
        endpoint_base(std::shared_ptr<memory> m) : mem_(m) {}
        std::shared_ptr<memory> get_memory()
        {
            return mem_;
        }
        virtual ~endpoint_base() = default;

    private:
        std::shared_ptr<memory> mem_;
    };

} // namespace hcpp

#endif