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
#include <asio/signal_set.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include <thread>

#include "httpserver.h"
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

int main(int argc, char **argv)
{
    try
    {
        namespace log = spdlog;
        spdlog::set_level(spdlog::level::debug);
        spdlog::cfg::load_env_levels();
        // spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v ***");
        // spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v");
        spdlog::set_pattern("\033[1;37m[%Y-%m-%d %H:%M:%S.%e] [%t] [%^%l%$]\033[0m %v");
        spdlog::debug("hcpp launch");

        std::string cfg_path = hcpp::config::CONFIG_DEFAULT_PATH;
        if (argc == 2)
        {
            cfg_path = argv[1];
        }
        auto c = hcpp::config::get_config(cfg_path);
        c->config_to(hcpp::slow_dns::get_slow_dns());

        asio::io_context io_context;

        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
                           { io_context.stop(); });

        hcpp::httpserver hs;
        hcpp::mimt_https_server mhs;
        if (!c->config_to(mhs))
        {
            return -1;
        }
        mhs.set_ch(std::make_shared<hcpp::socket_channel>(io_context, 10));

        hs.attach_tunnel([&mhs](auto &&c, auto h, auto s)
                         {
                    if(auto r=mhs.find_tunnel(h,s);r){
                        return *r;
                    }
                      return c; });

        auto exit_handler = [&io_context](auto &&eptr)
        {
            try
            {
                if (eptr)
                    std::rethrow_exception(eptr);
            }
            catch (const std::exception &e)
            {
                log::error("exit_handler: {}", e.what());
                io_context.stop();
            }
        };

        co_spawn(io_context, hs.wait_http(c->get_port(),io_context), exit_handler);
        co_spawn(io_context, mhs.wait_c(c->get_proxy_service()), exit_handler);

        auto create_thread = [&](auto self, int i) -> void
        {
            if (i > 0)
            {
                spdlog::debug("线程{}创建成功", i);
                std::jthread j(self, self, i - 1);
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
                j.join();
                spdlog::debug("线程{}退出成功", i);
            }
        };
        // 为了防止对象在多线程情况下销毁出问题
        // std::jthread t(create_thread, create_thread, 1);

        io_context.run();
    }
    catch (std::exception &e)
    {
        spdlog::error(e.what());
    }
}
