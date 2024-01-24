#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "dnshttps.h"
#include "net-headers.h"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <asio.hpp>
#include <asio/ssl.hpp>

#include <arpa/inet.h>

#include <thread>

using namespace asio;
using namespace harddns;
using tcp_resolver = asio::use_awaitable_t<>::as_default_on_t<asio::ip::tcp::resolver>;

awaitable<void> dns_lookup(string name,io_context &c)
{
    tcp_resolver resolver(c);
    // auto dns_host="rubyfish.cn";
    // 可以
    auto dns_host="1.1.1.1";
    // 可以
    // auto dns_host = "doh.360.cn";
    // auto dns_host="dns.alidns.com";
    // auto dns_host="doh.pub";
    // auto dns_host="doh.opendns.com";
    // auto dns_host="dns-unfiltered.adguard.com";
    auto endpoints = resolver.resolve(dns_host, "https");
    // auto endpoints = resolver.resolve("dns.alidns.com", "https");
    asio::ssl::context ctx(asio::ssl::context::sslv23);

    tcp_socket s(c);
    co_await async_connect(s, endpoints);
    ssl::stream<tcp_socket> ss(std::move(s), ctx);
    co_await ss.async_handshake(asio::ssl::stream_base::client);
    dnshttps dd(std::move(ss), dns_host);

    dnshttps::dns_reply dr;

    std::string res;
    co_await dd.get(name, htons(net_headers::dns_type::AAAA), dr, res);
}

TEST_CASE("example")
{

    asio::io_context io_context;

    // co_spawn(io_context, dns_lookup("huya.com",io_context), asio::detached);
    // co_spawn(io_context, dns_lookup("github.com",io_context), asio::detached);
    co_spawn(io_context, dns_lookup("www.baidu.com",io_context), asio::detached);

    io_context.run();
    // std::this_thread::sleep_for(std::chrono::seconds(3));
}