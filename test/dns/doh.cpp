#include "doctest/doctest.h"
#include "dns/dnshttps.h"
#include "dns/net-headers.h"
#include "https/ssl_socket_wrap.h"

#include <iostream>

#include <asio/connect.hpp>

#include <thread>

using namespace asio;
using namespace harddns;
using namespace hcpp;
using tcp_resolver = asio::use_awaitable_t<>::as_default_on_t<asio::ip::tcp::resolver>;

awaitable<void> dns_lookup(string name, io_context &c)
{
    // auto dns_host="rubyfish.cn";
    // 可以
    auto dns_host = "1.1.1.1";
    // 可以
    // auto dns_host = "doh.360.cn";
    // auto dns_host="dns.alidns.com";
    // auto dns_host="doh.pub";
    // auto dns_host="doh.opendns.com";
    // auto dns_host="dns-unfiltered.adguard.com";
    tcp_resolver resolver(c);
    auto endpoints = resolver.resolve(dns_host, "https");
    // auto endpoints = resolver.resolve("dns.alidns.com", "https");
    tcp_socket s(c);
    co_await asio::async_connect(s, endpoints);

    auto ssl_m = std::make_shared<ssl_sock_mem>(asio::ssl::stream_base::client);
    ssl_m->init(std::move(s));
    co_await ssl_m->async_handshake();
    co_await ssl_m->wait();

    dnshttps dd(ssl_m, dns_host);

    dnshttps::dns_reply dr;

    co_await dd.get_A(name, dr);
}

TEST_CASE("example")
{

    asio::io_context io_context;

    // co_spawn(io_context, dns_lookup("huya.com",io_context), asio::detached);
    // co_spawn(io_context, dns_lookup("github.com",io_context), asio::detached);
    co_spawn(io_context, dns_lookup("www.baidu.com", io_context), asio::detached);

    io_context.run();
    // std::this_thread::sleep_for(std::chrono::seconds(3));
}