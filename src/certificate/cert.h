#ifndef SRC_CERTIFICATE_CERT
#define SRC_CERTIFICATE_CERT

#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include <string>
#include <vector>

namespace hcpp
{
    struct name_entry
    {
        int nid_;
        const char *nid_val_;
        int nid_val_len_;
    };

    const inline std::vector<name_entry> default_name_entries = {
        {NID_countryName, "CN", sizeof("CN") - 1},
        {NID_organizationName, "NoBody", sizeof("NoBody") - 1},
        {NID_organizationalUnitName, "JS", sizeof("JS") - 1},
        {NID_stateOrProvinceName, "JS", sizeof("JS") - 1},
        {NID_commonName, "SQ", sizeof("SQ") - 1},
        {NID_localityName, "SQ", sizeof("SQ") - 1},
    };

    X509 *make_x509();
    EVP_PKEY *make_pkey();
    void set_version(X509 *cert);
    void set_serialNumber(X509 *cert);
    void set_issuer(X509 *cert, const std::vector<name_entry> &ne = default_name_entries);
    void set_validity(X509 *cert, std::size_t days = 365 * 10);
    void set_subject(X509 *cert, const std::vector<name_entry> &ne = default_name_entries);
    void set_pubkey(X509 *cert, EVP_PKEY *pkey);
    void add_ca_key_usage(X509 *cert);
    void add_SKI(X509 *cert);
    void add_AKI(X509 *cert);
    void add_DNS_SAN(X509 *cert, const std::vector<std::string> &dns_names);
    void add_ca_BS(X509 *cert);
    void sign(X509 *cert, EVP_PKEY *pkey);

    std::string make_pem_str(X509 *cert);
    std::string make_pem_str(EVP_PKEY *pkey);
    std::string make_pem_str(X509_PUBKEY *pkey);

    X509 *make_x509(const std::string_view cert);
    EVP_PKEY *make_pkey(const std::string_view pkey);

    void add_issuer(X509 *cert, X509 *ca_cert);
} // namespace hcpp

#endif /* SRC_CERTIFICATE_CERT */
