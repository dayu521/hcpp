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
#include "config.h"

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
    auto executor = co_await this_coro::executor;

    auto cc = std::make_shared<hcpp::socket_channel>(executor, 10);

    // co_spawn(executor,hcpp::https_listen(cc),detached);

    tcp_acceptor acceptor(executor, {tcp::v4(), hcpp::config::get_config()->get_port()});
    auto d = acceptor.local_endpoint();
    spdlog::debug("服务器监听端口:{}", d.port());
    for (;;)
    {
        auto socket = co_await acceptor.async_accept();
        co_spawn(executor, http_proxy(hcpp::http_client(std::move(socket)), cc), detached);
    }
}

int main(int argc, char **argv)
{
    try
    {
        spdlog::set_level(spdlog::level::debug);
        spdlog::cfg::load_env_levels();
        // spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
        // spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v");
        spdlog::set_pattern("\033[1;37m[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$]\033[0m %v");
        spdlog::debug("hcpp launch");

        asio::io_context io_context;

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
                           {
                            io_context.stop(); });

        std::string cfg_path = hcpp::config::CONFIG_DEFAULT_PATH;
        if (argc == 2)
        {
            cfg_path = argv[1];
        }
        auto c = hcpp::config::get_config(cfg_path);
        c->load_host_mapping(hcpp::slow_dns::get_slow_dns());
        c->save_callback([sd=hcpp::slow_dns::get_slow_dns()](auto &&cs)
                         { sd->save_hm(cs.hm_); });

        auto create_thread = [&io_context](auto self, int i) -> void
        {
            if (i > 0)
            {
                auto service = [&io_context]()
                {
                    while (!io_context.stopped())
                    {
                        try
                        {
                            io_context.run();
                        }
                        catch (const std::exception &e)
                        {
                            spdlog::error(e.what());
                        }
                    }
                    spdlog::debug("线程退出成功");
                };
                std::thread t(service);
                t.detach();
                i--;
                self(self, i);
            }
        };
        create_thread(create_thread, 3);

        co_spawn(io_context, listener(), [](std::exception_ptr eptr)
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
