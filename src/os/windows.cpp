#include "common.h"

#include <wincrypt.h>

#include <stdexcept>

#include <openssl/ssl.h>

#include <spdlog/spdlog.h>

namespace hcpp
{
    namespace log = spdlog;

    //https://stackoverflow.com/questions/9507184/can-openssl-on-windows-use-the-system-certificate-store?rq=3
    //https://www.coder.work/article/1710481
    struct cert_store
    {
        X509_STORE *store_ = nullptr;
        cert_store()
        {
            log::error("cert_store构造函数");
            try
            {
                auto hCertStore = CertOpenSystemStore(0, "ROOT");
                if (!hCertStore)
                {
                    throw std::runtime_error("CertOpenSystemStore 失败");
                }
                store_ = X509_STORE_new();

                PCCERT_CONTEXT pCertContext = nullptr;
                while (true)
                {
                    pCertContext = CertEnumCertificatesInStore(hCertStore, pCertContext);
                    if (pCertContext == nullptr)
                    {
                        break;
                    }
                    // https://www.openssl.org/docs/manmaster/man3/d2i_X509.html
                    X509 *cert = d2i_X509(nullptr, (const unsigned char **)&pCertContext->pbCertEncoded, pCertContext->cbCertEncoded);
                    if (cert == nullptr)
                    {
                        throw std::runtime_error("d2i_X509 获取证书失败");
                    }
                    X509_STORE_add_cert(store_, cert);
                    X509_free(cert);
                }
                if (CertFreeCertificateContext(pCertContext) == 0)
                {
                    throw std::runtime_error("CertFreeCertificateContext 释放pCertContext失败");
                }
                if (!CertCloseStore(hCertStore, 0))
                {
                    throw std::runtime_error("CertCloseStore 失败");
                }
            }
            catch (...)
            {
                X509_STORE_free(store_);
                throw;
            }
        }
        ~cert_store()
        {
            //BUG 为什么不调用  还要保没有其它人拥有此store_
            log::error("cert_store析构函数");
            X509_STORE_free(store_);
        }
    };
    // https://www.coder.work/article/1710481
    // https://learn.microsoft.com/zh-cn/windows/win32/api/wincrypt/nf-wincrypt-certopensystemstorea
    void set_platform_default_verify_store(ssl::context &ctx)
    {
        static cert_store cs{};
        SSL_CTX_set1_cert_store(ctx.native_handle(), cs.store_);
    }
} // namespace hcpp
