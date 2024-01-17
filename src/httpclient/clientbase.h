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
        //XXX读取最大max_n的数据,放到buff中.可以提供数据存放在buff的开始点
        virtual awaitable<std::size_t> async_read_some(std::string & buff,std::size_t max_n,std::size_t begin=0) = 0;
        virtual awaitable<std::size_t> async_write_some(const std::string &) = 0;

        //XXX 读直到.返回已读到的字符串,和剩下在缓存中的数量
        virtual awaitable<std::pair<std::string,std::size_t>> async_read_until(const std::string &) = 0;
        //XXX 从缓存中取回读直到的数据
        virtual std::string read_until_left(std::size_t max_n)=0;
        virtual awaitable<void> async_write_all(const std::string &) = 0;
        virtual bool eof() = 0;

    public:
        virtual ~memory() = default;
    };

    template <typename T>
    awaitable<void> read(std::shared_ptr<memory> m, T &t);

    template <typename T>
    concept request_concept = requires(T t) {
        {
          true  // co_await t.do_read()
        };
    };

    template <typename... T>
    struct request;

    template <request_concept T0, typename... T1>
    struct request<T0, T1...>
    {
        awaitable<std::pair<T0, request<T1...>>> wait()
        {
            T0 r;
            co_await read(mem_, r);
            co_return std::make_pair(std::move(r), request<T1...>(mem_));
        }

        request(std::shared_ptr<memory> mem) : mem_(mem) {}

    protected:
        std::shared_ptr<memory> mem_;
    };

    template <request_concept T>
    struct request<T>
    {
        awaitable<T> wait()
        {
            T r;
            co_await read(mem_, r);
            co_return std::move(r);
        }

    protected:
        std::shared_ptr<memory> mem_;
    };

    struct http_request
    {
        using http_msg_line = std::string;
        using reuqest_line_ = http_msg_line;
        using http_msg_body = std::vector<std::string>;
        using http_headers = std::unordered_map<std::string, http_msg_line>;
        using eof=std::string;

        reuqest_line_ line_;
        http_headers headers_;
        http_msg_body bodys_;
    };

    class client_base
    {
    public:
        client_base(std::shared_ptr<memory> m) : mem_(m) {}
        // awaitable<void> wait_request();
        using request_builder = request<http_request::reuqest_line_, http_request::http_headers, http_request::http_msg_body>;
        request_builder get_request()
        {
            return request_builder(mem_);
        }
        ~client_base() = default;

    private:
        std::shared_ptr<memory> mem_;
    };

} // namespace hcpp

#endif