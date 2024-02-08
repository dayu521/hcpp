#ifndef SRC_CERTIFICATE_CERT
#define SRC_CERTIFICATE_CERT

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include <string>

namespace hcpp
{
    X509 * make_x509();
    EVP_PKEY * make_pkey();
    void set_version(X509 *cert);
    void set_serialNumber(X509 *cert);
    void add_issuer(X509 *cert,std::string_view name);
    void set_validity(X509 *cert,std::size_t days=365*10);
    void set_subject(X509 *cert);
    void set_pubkey(X509 *cert, EVP_PKEY *pkey);
    void add_ca_key_usage(X509 *cert);
    void add_SKI(X509 *cert);
    void add_AKI(X509 *cert);
    void add_DNS_SAN(X509 *cert, std::string_view dns_name);
    void add_ca_BS(X509 *cert);
    void sign(X509 *cert, EVP_PKEY *pkey);

    std::string make_pem_str(X509 *cert);
    std::string make_pem_str(EVP_PKEY *pkey);
} // namespace hcpp

#endif /* SRC_CERTIFICATE_CERT */
