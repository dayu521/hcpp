#include <asio/experimental/coro.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>

#include <spdlog/spdlog.h>

#include <doctest/doctest.h>

using namespace asio;

experimental::coro<int(int)> coro_func(any_io_executor e)
{
    int i = 0;
    while (true)
    {
        i += co_yield i;
    }
}

experimental::coro<void> a(auto &c)
{
    auto v = co_await c(11);
    spdlog::info("a {}", *v);
}

experimental::coro<void> b(auto &c)
{
    auto v = co_await c(22);
    spdlog::info("b {}", *v);
}

TEST_CASE("example")
{
    io_context io_context;
    auto c = coro_func(io_context.get_executor());
    co_spawn(a(c), detached);
    co_spawn(b(c), detached);
    io_context.run();
}