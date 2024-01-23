#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "dnshttps.h"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <asio.hpp>
#include <asio/ssl.hpp>

#include<thread>

using namespace asio;
using namespace harddns;
using tcp_resolver = asio::use_awaitable_t<>::as_default_on_t<asio::ip::tcp::resolver>;

awaitable<void> dns_lookup(io_context  & c)
{
    tcp_resolver resolver(c);
    auto endpoints = resolver.resolve("dns.alidns.com", "https");
    asio::ssl::context ctx(asio::ssl::context::sslv23);

    tcp_socket s(c);
    co_await async_connect(s,endpoints);
    ssl::stream<tcp_socket> ss(std::move(s), ctx);
    co_await ss.async_handshake(asio::ssl::stream_base::client);
    dnshttps dd(std::move(ss));

    dnshttps::dns_reply dr;

    // co_await dd.get("https://dns.alidns.com/dns-query", "/dns-query");
    std::string res;
    co_await dd.get("github.com", 1,dr,res);
}

TEST_CASE("example")
{

    asio::io_context io_context;

    co_spawn(io_context, dns_lookup(io_context), asio::detached);

    io_context.run();
    std::this_thread::sleep_for(std::chrono::seconds(3));
}