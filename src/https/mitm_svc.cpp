#include "mitm_svc.h"
#include "http/tunnel.h"
#include "certificate/cert.h"
#include "dns.h"

#include <thread>

#include <spdlog/spdlog.h>
#include <asio/buffer.hpp>
#include <asio/bind_executor.hpp>

#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace hcpp
{
    namespace log = spdlog;

    using namespace asio::buffer_literals;

    awaitable<std::shared_ptr<memory>> mitm_svc::wait(std::string svc_host, std::string svc_service,std::shared_ptr<InterceptSet> is)
    {
        if (!m_)
        {
            throw std::runtime_error("mitm_svc::wait: m_ is nullptr");
        }
        co_return m_;
    }

    mitm_svc::mitm_svc() : http_svc_keeper(make_threadlocal_svc_cache<mitm_svc>(), slow_dns::get_slow_dns())
    {
        f_ = make_mem_factory();
    }
    void mitm_svc::set_sni_host(std::string_view host)
    {
        sni_host_ = host;
    }
    void mitm_svc::close_sni()
    {
        enable_sni_ = false;
    }
    void mitm_svc::add_SAN_collector(part_cert_info &pci)
    {
        // verify_fun_=
        verify_fun_ = [&pci](bool preverified, auto &v_ctx)
        {
            if (preverified)
            {
                X509 *cert = X509_STORE_CTX_get_current_cert(v_ctx.native_handle());
                // X509_NAME *subject = X509_get_subject_name(cert);
                // log::error("{}", X509_NAME_oneline(subject, nullptr, 0));
                // 获取目标主机证书
                // echo -n | openssl s_client -connect gitee.com:443 | sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p'  | openssl x509 -text -noout
                // 获取目标主机证书中的subjectAltName
                // https://www.openssl.org/docs/manmaster/man3/X509_get_ext_d2i.html
                if (auto names = (GENERAL_NAMES *)X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr); names != nullptr)
                {

                    for (int j = 0; j < sk_GENERAL_NAME_num(names); j++)
                    {
                        GENERAL_NAME *name = sk_GENERAL_NAME_value(names, j);
                        if (name->type == GEN_DNS)
                        {
                            // 如果是DNS类型的Subject Alternative Name
                            // 获取DNS名称
                            char *dns_name = (char *)ASN1_STRING_get0_data(name->d.dNSName);
                            log::info("DNS Name: {}", dns_name);
                            pci.dns_name_.push_back(dns_name);
                        }
                        // 可以根据需要处理其他类型的Subject Alternative Name
                    }
                    //XXX 一般服务器会发送证书链,包含多个证书,也就是校验函数会被调用多次
                    //我们只对这些有主体名(SAN)的证书创建假证书
                    pci.pubkey_ = make_pem_str(X509_get_X509_PUBKEY(cert));
                }
                return true;
            }
            else
            {
                return false;
            }
            //  X509_NAME_oneline(X509_get_subject_name(cert), subject_name, sizeof(subject_name)); });
        };
    }

    awaitable<void> mitm_svc::make_memory(std::string svc_host, std::string svc_service,bool use_doh)
    {
        try
        {
            if (auto sock = co_await make_socket(svc_host, svc_service,use_doh); sock)
            {
                auto ssl_m = std::make_shared<ssl_sock_mem>(svc_host, svc_service);
                ssl_m->init_client(std::move(*sock));
                if (!enable_sni_)
                {
                    ssl_m->close_sni();
                }
                else if (!sni_host_.empty())
                {
                    ssl_m->set_sni(sni_host_);
                }
                // XXX 这里不能用shared_from_this,因为回调会一直保持当前对象的智能指针,不释放
                auto verify_fun = [verify_fun = verify_fun_, host = svc_host](bool preverified, auto &v_ctx)
                {
                    if (ssl::host_name_verification(host)(preverified, v_ctx))
                    {
                        if (verify_fun)
                        {
                            return verify_fun(preverified, v_ctx);
                        }
                        return true;
                    }
                    else
                    {
                        return false;
                    }
                };
                ssl_m->set_verify_callback(verify_fun);
                // XXX 这里的async_handshake会阻塞住线程,因此要在单独的协程里再进行.所以当前函数不能调用在监听连接的时候
                co_await ssl_m->async_handshake();
                m_ = ssl_m;
                co_return;
            }
            log::error("mitm_svc::wait:socket 创建失败 {}", svc_host);
            throw std::runtime_error("mitm_svc::wait:socket 创建失败");
        }
        catch (std::exception &e)
        {
            log::error("mitm_svc::wait:{} {}", svc_host, e.what());
            throw;
        }
    }
    subject_identify mitm_svc::make_fake_server_id(const std::vector<std::string> &dns_name, std::shared_ptr<subject_identify> ca_si)
    {
        auto ca_pky = make_pkey(ca_si->pkey_pem_);
        auto ca_cert = make_x509(ca_si->cert_pem_);

        auto cert = make_x509();
        auto pky = make_pkey();
        set_version(cert);
        set_serialNumber(cert);
        // 默认3650天
        set_validity(cert);

        set_subject(cert);
        // 设置目标服务器的dns
        add_DNS_SAN(cert, dns_name);

        add_AKI(cert);
        add_SKI(cert);
        // 设置密钥的公钥
        set_pubkey(cert, pky);
        // 设置发行者为ca
        add_issuer(cert, ca_cert);
        // 使用ca签名
        sign(cert, ca_pky);

        subject_identify si{make_pem_str(pky), make_pem_str(cert)};

        X509_free(cert);
        EVP_PKEY_free(ca_pky);

        return si;
    }
    awaitable<std::shared_ptr<memory>> ssl_mem_factory::create(std::string host, std::string service)
    {
        try
        {
            // if (auto sock = co_await make_socket(host, service); sock)
            // {
            //     auto ssl_m = std::make_shared<ssl_sock_mem>(host, asio::ssl::stream_base::client);
            //     ssl_m->init(std::move(*sock));
            //     // ssl_m->set_sni("www.baidu.com");
            //     ssl_m->close_sni();
            //     co_await ssl_m->async_handshake();
            //     co_return ssl_m;
            // }
            co_return std::shared_ptr<memory>{};
        }
        catch (std::exception &e)
        {
            log::error("ssl_mem_factory::create:{} {}", host, e.what());
            throw;
        }
    }

    awaitable<void> channel_tunnel::make_tunnel(std::shared_ptr<memory> m, std::string host, std::string service)
    {
        co_await m->async_write_all("HTTP/1.1 200 OK\r\n\r\n");
        chc_ = std::make_shared<channel_client>();
        chc_->host_ = host;
        chc_->service_ = service;
        chc_->is_=is_;
        m->check(*this);
        co_await channel_->async_send(asio::error_code{}, chc_);
    }

    void channel_tunnel::make(std::unique_ptr<tcp_socket> sock)
    {
        chc_->sock_ = std::move(sock);
    }

    void channel_tunnel::make(std::unique_ptr<ssl_socket> sock)
    {
        log::error("未实现");
        sock->shutdown();
        chc_->sock_ = std::unique_ptr<tcp_socket>();
    }

    http_request https_client::make_request() const
    {
        http_request hr;
        hr.host_ = host_;
        hr.port_ = service_;
        return hr;
    }

    awaitable<void> https_client::init()
    {
        assert(init_);
        co_await init_(*this);
    }

    awaitable<std::shared_ptr<memory>> channel_client::make(subject_identify si) &&
    {
        auto e = co_await this_coro::executor;
        auto &&protocol = sock_->local_endpoint().protocol();
        auto &&h = sock_->release();
        tcp_socket s(e, protocol, std::move(h));
        auto ep = s.local_endpoint();
        auto ssl_m = std::make_shared<ssl_sock_mem>(ep.address().to_string(), std::to_string(ep.port()));
        ssl_m->init_server(std::move(s), std::move(si));
        co_await ssl_m->async_handshake();
        co_return ssl_m;
    }

} // namespace hcpp
