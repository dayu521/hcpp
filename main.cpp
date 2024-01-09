//
// echo_server_with_default.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2023 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// #define ASIO_SEPARATE_COMPILATION
// #include <asio/impl/src.hpp>

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/signal_set.hpp>
#include <cstdio>
#include <asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include <thread>

#include "proxy.h"
#include "dns.h"

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable_t;
using asio::ip::tcp;
using tcp_acceptor = use_awaitable_t<>::as_default_on_t<tcp::acceptor>;
using tcp_socket = use_awaitable_t<>::as_default_on_t<tcp::socket>;
namespace this_coro = asio::this_coro;

awaitable<void> listener()
{
    // std::cout << std::this_thread::get_id() << std::endl;
    auto executor = co_await this_coro::executor;

    tcp_acceptor acceptor(executor, {tcp::v4(), 55555});
    auto d = acceptor.local_endpoint();
    spdlog::debug("服务器监听端口:{}", d.port());
    for (;;)
    {
        auto socket = co_await acceptor.async_accept();
        co_spawn(executor, read_http_input(std::move(socket), hcpp::slow_dns::get_slow_dns()), detached);
    }
}

int main()
{
    try
    {
        spdlog::set_level(spdlog::level::debug);
        spdlog::cfg::load_env_levels();
        spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
        spdlog::debug("hcpp launch");

        asio::io_context io_context;
        // asio::io_context io_context(ASIO_CONCURRENCY_HINT_UNSAFE_IO);

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
                           {
                            hcpp::slow_dns::get_slow_dns()->save_mapping(); 
                            io_context.stop(); });

        hcpp::slow_dns::get_slow_dns()->init_resolver(io_context.get_executor());

        auto create_thread = [&io_context](auto self, int i) -> void
        {
            if (i > 0)
            {
                std::thread t([&io_context]()
                              {
                                while(!io_context.stopped()){
                                  try
                                  {
                                      io_context.run();
                                  }
                                  catch (const std::exception &e)
                                  {
                                      spdlog::error(e.what());
                                  } 
                                } });
                t.detach();
                i--;
                self(self, i);
            }
        };
        create_thread(create_thread, 3);

        co_spawn(io_context, listener(), [&io_context](std::exception_ptr eptr)
                 {
                     // try
                     // {
                     if (eptr)
                         std::rethrow_exception(eptr);
                     // }
                     // catch(const std::exception& e)
                     // {
                     //     std::cout << "Caught exception: '" << e.what() << "'\n";
                     //     io_context.stop();
                     // }
                 });

        io_context.run();
    }
    catch (std::exception &e)
    {
        std::printf("Exception: %s\n", e.what());
    }
}
