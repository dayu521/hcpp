#ifndef SRC_HMEMORY
#define SRC_HMEMORY
#include <cstddef>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <utility>

#include "untrust/kmp.h"

#include <asio/awaitable.hpp>
#include <spdlog/spdlog.h>

namespace hcpp
{
    using namespace asio;

    namespace log=spdlog;

    class mem_move;

    // TODO 抽象出buff
    class memory
    {
    public:
        virtual awaitable<bool> wait() = 0;

        // XXX读取最大max_n的数据,放到buff中.
        virtual awaitable<std::string_view> async_load_some(std::size_t max_n = 1024 * 4) { co_return ""; }
        virtual awaitable<std::size_t> async_write_some(std::string_view) { co_return 0; }

        // TODO 包含缓存的直到,首先从buff中查找,没有再加载.但这改变了函数的语义
        //  XXX 读直到.返回已读到的字符串.同时把读到的所有字符放到buff中,通过get_some remove_some 进行缓存读取和消耗
        virtual awaitable<std::string_view> async_load_until(const std::string &) { co_return ""; }
        virtual awaitable<void> async_write_all(std::string_view) { co_return; }
        virtual std::string_view get_some() { return {}; }
        virtual std::size_t remove_some(std::size_t index) { return 0; }
        // XXX 返回0说明不需要合并
        virtual std::size_t merge_some() { return 0; }
        // 读写状态是否合法
        virtual bool ok() { return read_ok_ && write_ok_; }
        virtual void make_alive() { long_time_ = true; }
        virtual bool alive() { return long_time_; }
        virtual void reset() {}

    public:
        virtual ~memory() = default;
        virtual void check(mem_move &) {}

    protected:
        bool read_ok_ = true;
        bool write_ok_ = true;
        bool long_time_ = false;
    };

    class mem_factory
    {
    public:
        virtual awaitable<std::shared_ptr<memory>> create(std::string host, std::string service) = 0;
        virtual ~mem_factory() = default;
    };

    class simple_mem : public memory
    {
    private:
        static void expection_impl() { throw std::runtime_error("to implement"); }

    public:
        virtual awaitable<bool> wait()
        {
            expection_impl();
            co_return false;
        }

        // XXX读取最大max_n的数据,放到buff中.
        virtual awaitable<std::string_view> async_load_some(std::size_t max_n = 1024 * 4)
        {
            expection_impl();
            co_return "";
        }
        virtual awaitable<std::size_t> async_write_some(std::string_view)
        {
            expection_impl();
            co_return 0;
        }

        // XXX 读直到.返回已读到的字符串.同时把读到的所有字符放到buff中,通过get_some remove_some 进行缓存读取和消耗
        virtual awaitable<std::string_view> async_load_until(const std::string &)
        {
            expection_impl();
            co_return "";
        }
        virtual awaitable<void> async_write_all(std::string_view)
        {
            expection_impl();
            co_return;
        }
    };

    // TODO
    /***********下面的方法属于buff接口***********/
    inline awaitable<std::size_t> transfer_mem(std::shared_ptr<hcpp::memory> from, std::shared_ptr<hcpp::memory> to, std::size_t total_bytes)
    {
        decltype(total_bytes) nx = 0;
        from->merge_some();
        auto som_str = from->get_some();

        if (nx < total_bytes)
        {
            if (som_str.size() > 0)
            {
                if (som_str.size() + nx > total_bytes)
                {
                    som_str = som_str.substr(0, total_bytes - nx);
                }
                co_await to->async_write_all(som_str);
                nx += som_str.size();
                from->remove_some(som_str.size());
            }
        }

        while (nx < total_bytes)
        {
            auto str = co_await from->async_load_some(total_bytes - nx);
            co_await to->async_write_all(str);
            nx += str.size();
            from->remove_some(str.size());
        }
        assert(from->merge_some() == 0);
        co_return nx;
    }
    inline awaitable<std::size_t> transfer_mem_until(std::shared_ptr<hcpp::memory> from, std::shared_ptr<hcpp::memory> to, const std::string &pattern)
    {
        std::size_t n = 0;
        from->merge_some();
        auto som_str = from->get_some();

        untrust::KMP kmp(pattern);
        // 缓存没有,可以直接加载
        // 匹配成功,p是模式在字符串中的起始索引.否则是最后匹配失败所在的索引
        // AA匹配 ABA,返回2
        if (auto p = kmp.search(som_str, pattern); p < 0)
        {
            co_await to->async_write_all(som_str);
            n += som_str.size();
            from->remove_some(som_str.size());

            auto line = co_await from->async_load_until(pattern);
            co_await to->async_write_all(line);
            n += line.size();
            from->remove_some(line.size());
        }
        else if (som_str.size() - p < pattern.size())
        {
            som_str = som_str.substr(0, p);
            co_await to->async_write_all(som_str);
            n += som_str.size();
            from->remove_some(som_str.size());

            log::error("transfer_mem_until: 这个暂未实现");
            throw std::runtime_error("低概率发生了,暂时不处理");
            // TODO 脱离这个分支到另外两个分支就好
            //  std::string tmp=from->get_some();
            //  while (tmp.size()- p < pattern.size())
            //  {
            //      /* code */
            //  }
        }
        else // 缓存有,直接写缓存
        {
            co_await to->async_write_all(som_str);
            n += som_str.size();
            from->remove_some(som_str.size());
        }
        co_return n;
    }
} // namespace hcpp

#endif /* SRC_HMEMORY */
