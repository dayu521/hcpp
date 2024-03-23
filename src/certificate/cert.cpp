#include "cert.h"

#include <cstring>
#include <stdexcept>

#include <openssl/bio.h>

// X509_new
namespace hcpp
{

    // 创建pkey
    EVP_PKEY *generate_pkey()
    {
        // 创建key
        EVP_PKEY_CTX *ctx;
        EVP_PKEY *pkey = NULL;

        ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
        if (!ctx)
            /* Error occurred */
            return nullptr;
        if (EVP_PKEY_keygen_init(ctx) <= 0)
            /* Error */
            return nullptr;
        if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0)
            /* Error */
            return nullptr;
        /* Generate key */
        if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
            /* Error */
            return nullptr;

        return pkey;
    }

    X509 *make_x509()
    {
        return X509_new();
    }

    EVP_PKEY *make_pkey()
    {
        return generate_pkey();
    }

    // Version
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.1
    void set_version(X509 *cert)
    {
        // X509_set_version
        X509_set_version(cert, X509_VERSION_3);
    }

    // Serial Number
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.2
    void set_serialNumber(X509 *cert)
    {
        unsigned char buff[20];
        if (RAND_bytes(buff, sizeof(buff)) != 1)
        {
            // 随机数生成失败，处理错误
        }
        buff[0] &= 0x7f; /* Ensure positive serial! */

        BIGNUM *serial_bn = BN_bin2bn(buff, sizeof(buff), NULL);
        if (serial_bn == NULL)
        {
            // 转换失败，处理错误
        }
        auto *serial = ASN1_INTEGER_new();
        BN_to_ASN1_INTEGER(serial_bn, serial);

        // X509_set_serialNumber >=3.2.0
        X509_set_serialNumber(cert, serial);

        ASN1_INTEGER_free(serial);
        BN_free(serial_bn);
    }

    // Issuer
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.4
    void set_issuer(X509 *cert, const std::vector<name_entry> &ne)
    {
        //   * country,
        //   * organization,
        //   * organizational unit,
        //   x distinguished name qualifier,
        //   * state or province name,
        //   * common name (e.g., "Susan Housley"), and
        //   x serial number.

        //   * locality

        auto name = X509_NAME_new();

        for (auto &&i : ne)
        {
            X509_NAME_add_entry_by_NID(name, i.nid_, MBSTRING_ASC, (const unsigned char *)i.nid_val_, i.nid_val_len_, -1, 0);
        }
        // X509_set_issuer_name
        X509_set_issuer_name(cert, name);
        X509_NAME_free(name);
    }

    // Validity
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.5
    void set_validity(X509 *cert, std::size_t days)
    {
        // 这两个时间点都是包含的
        auto begin_time = ASN1_TIME_new();
        auto end_time = ASN1_TIME_new();
        // now
        X509_time_adj_ex(begin_time, 0, 0, nullptr);
        X509_time_adj_ex(end_time, days, 0, nullptr);

        X509_set1_notBefore(cert, begin_time);
        X509_set1_notAfter(cert, end_time);
    }

    // Subject
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.6
    void set_subject(X509 *cert, const std::vector<name_entry> &ne)
    {
        auto name = X509_NAME_new();

        for (auto &&i : ne)
        {
            X509_NAME_add_entry_by_NID(name, i.nid_, MBSTRING_ASC, (const unsigned char *)i.nid_val_, i.nid_val_len_, -1, 0);
        }
        X509_set_subject_name(cert, name);
        X509_NAME_free(name);
        // X509_set_subject_name 	get X509_NAME hashes or get and set issuer or subject names
        // X509_set_subject_name(cert, X509_get_issuer_name(cert));
    }

    // Subject Public Key Info
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.7
    void set_pubkey(X509 *cert, EVP_PKEY *pkey)
    {
        X509_set_pubkey(cert, pkey);
    }

    /*********扩展**********/
    // key usage
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.3
    void add_ca_key_usage(X509 *cert)
    {
        auto bb = ASN1_BIT_STRING_new();
        // ASN1_BIT_STRING_set_bit(bb, 6, 1);
        // ASN1_BIT_STRING_set_bit(bb, 5, 1);
        char8_t bv = KU_KEY_CERT_SIGN | KU_CRL_SIGN;
        ASN1_BIT_STRING_set(bb, (unsigned char *)&bv, sizeof(bv));
        X509_add1_ext_i2d(cert, NID_key_usage, bb, 1, X509_ADD_FLAG_DEFAULT);
        ASN1_BIT_STRING_free(bb);
    }

    void generate_key_id(X509 *cert, unsigned char *md, std::size_t *md_len)
    {
        // 不要释放
        // auto pubkey2 = X509_get0_pubkey(pkey);
        // EVP_sha
        // https://www.openssl.org/docs/manmaster/man3/X509_pubkey_digest.html
        X509_pubkey_digest(cert, EVP_sha1(), md, (unsigned int *)md_len);

        // EVP_PKEY_get_raw_public_key(pkey)
        // EVP_PKEY
        // or be replaced by (EVP_Q_digest(d, n, md, NULL, NULL, "SHA1", NULL) ? md : NULL)
        // SHA1();
    }
    // Subject Key Identifier
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.2
    void add_SKI(X509 *cert)
    {
        unsigned char md[EVP_MAX_MD_SIZE];
        std::size_t md_len = 0;
        generate_key_id(cert, md, &md_len);

        auto oct = ASN1_OCTET_STRING_new();
        ASN1_OCTET_STRING_set(oct, md, md_len);
        X509_add1_ext_i2d(cert, NID_subject_key_identifier, oct, 0, X509V3_ADD_DEFAULT);
        ASN1_OCTET_STRING_free(oct);
    }

    // Authority Key Identifier
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.1
    void add_AKI(X509 *cert)
    {
        unsigned char md[EVP_MAX_MD_SIZE];
        std::size_t md_len = 0;
        generate_key_id(cert, md, &md_len);

        auto oct = ASN1_OCTET_STRING_new();
        ASN1_OCTET_STRING_set(oct, md, md_len);

        auto akid = AUTHORITY_KEYID_new();
        akid->keyid = oct;
        akid->issuer = nullptr;
        akid->serial = nullptr;
        X509_add1_ext_i2d(cert, NID_authority_key_identifier, akid, 0, X509V3_ADD_DEFAULT);
        ASN1_OCTET_STRING_free(oct);
    }

    // Subject Alternative Name
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.6
    void add_DNS_SAN(X509 *caCert, const std::vector<std::string> &dns_names)
    {
        if (dns_names.empty())
        {
            return;
        }
        GENERAL_NAMES *gens = sk_GENERAL_NAME_new_null();

        for (auto &dns_name : dns_names)
        {
            GENERAL_NAME *gen_dns = GENERAL_NAME_new();
            ASN1_IA5STRING *ia5 = ASN1_IA5STRING_new();
            ASN1_STRING_set(ia5, dns_name.data(), dns_name.length());
            GENERAL_NAME_set0_value(gen_dns, GEN_DNS, ia5);
            sk_GENERAL_NAME_push(gens, gen_dns);
        }

        // in_addr_t ipv4 = inet_addr("10.0.0.1");
        // GENERAL_NAME *gen_ip = GENERAL_NAME_new();
        // ASN1_OCTET_STRING *octet = ASN1_OCTET_STRING_new();
        // ASN1_STRING_set(octet, &ipv4, sizeof(ipv4));
        // GENERAL_NAME_set0_value(gen_ip, GEN_IPADD, octet);
        // sk_GENERAL_NAME_push(gens, gen_ip);

        X509_add1_ext_i2d(caCert, NID_subject_alt_name, gens, 0, X509V3_ADD_DEFAULT);

        sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
    }

    // Basic Constraints
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.9
    void add_ca_BS(X509 *cert)
    {
        auto e1 = X509V3_EXT_conf_nid(nullptr, nullptr, NID_basic_constraints, "CA:true");
        X509_EXTENSION_set_critical(e1, 1);
        X509_add_ext(cert, e1, -1);
        // XXX为啥不正确
        //  auto bs = BASIC_CONSTRAINTS_new();
        //  bs->ca = 1;
        //  bs->pathlen = nullptr;
        //  X509_add1_ext_i2d(cert, NID_basic_constraints, bs, 1, X509V3_ADD_DEFAULT);
    }

    // 签名
    void sign(X509 *cert, EVP_PKEY *pkey)
    {
        X509_sign(cert, pkey, EVP_sha256());
    }

    std::string make_pem_str(X509 *cert)
    {
        BIO *bio = BIO_new(BIO_s_mem());
        PEM_write_bio_X509(bio, cert);
        auto cert_size = BIO_pending(bio);

        auto cert_array = new char[cert_size + 1];
        BIO_read(bio, cert_array, cert_size);
        BIO_free_all(bio);
        return cert_array;
    }

    std::string make_pem_str(EVP_PKEY *pkey)
    {
        BIO *bio = BIO_new(BIO_s_mem());
        PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL);
        auto pkey_size = BIO_pending(bio);
        std::string pkey_array{};
        pkey_array.resize(pkey_size+1);
        BIO_read(bio, pkey_array.data(), pkey_size);
        BIO_free_all(bio);
        return pkey_array;
    }

    std::string make_pem_str(X509_PUBKEY *pkey)
    {
        BIO *bio = BIO_new(BIO_s_mem());
        PEM_write_bio_X509_PUBKEY(bio, pkey);
        auto pkey_size = BIO_pending(bio);
        std::string pubkey_array;
        pubkey_array.resize(pkey_size + 1);
        BIO_read(bio, pubkey_array.data(), pkey_size);
        BIO_free_all(bio);
        return pubkey_array;
    }

    X509 *make_x509(const std::string_view cert)
    {
        auto bp = BIO_new_mem_buf(cert.data(), cert.size());
        if (bp == nullptr)
        {
            throw std::runtime_error("bio 打开失败");
        }
        auto c = PEM_read_bio_X509(bp, NULL, 0, NULL);
        if (c == nullptr)
        {
            throw std::runtime_error("make_x509 failed");
        }
        BIO_free(bp);
        return c;
    }

    EVP_PKEY *make_pkey(const std::string_view pkey)
    {
        auto bp = BIO_new_mem_buf(pkey.data(), pkey.size());
        if (bp == nullptr)
        {
            throw std::runtime_error("bio 打开失败");
        }
        auto p = PEM_read_bio_PrivateKey(bp, nullptr, nullptr, nullptr);
        if (p == nullptr)
        {
            throw std::runtime_error("读取密钥错误");
        }
        BIO_free(bp);
        return p;
    }

    void add_issuer(X509 *cert, X509 *ca_cert)
    {
        X509_set_issuer_name(cert, X509_get_subject_name(ca_cert));
    }

    // static void test()
    // {

    // auto cert = X509_new();

    // auto pkey = generate_pkey();
    // if (pkey == NULL)
    // CHECK(false);
    // print_key(pkey);

    // set_version(cert);
    // set_validity(cert);
    // set_serialNumber(cert);
    // add_issuer(cert);
    // set_subject(cert);
    // // add_SAN(cert);
    // add_ca_BS(cert);
    // add_AKI(cert);
    // add_SKI(cert);
    // add_ca_key_usage(cert);
    // set_pubkey(cert, pkey);
    // sign(cert, pkey);

    // print_cert(cert);

    // Save the private key to a file.
    // FILE *privateKeyFile = fopen("hcpp.key.pem", "w");
    // if (!privateKeyFile)
    // {
    //     // CHECK(false);
    // }
    // if (!PEM_write_PrivateKey(privateKeyFile, pkey, nullptr, nullptr, 0, nullptr, nullptr))
    // {
    //     fclose(privateKeyFile);
    //     // CHECK(false);
    // }
    // fclose(privateKeyFile);

    // // Save the CA certificate to a file.
    // // auto certfile="cert";

    // FILE *caCertFile = fopen("hcpp.crt.pem", "wb");
    // if (!caCertFile)
    // {
    //     X509_free(cert);
    //     // CHECK(false);
    // }
    // PEM_write_X509(caCertFile, cert);

    // fclose(caCertFile);
    // X509_free(cert);
    // }
} // namespace hcpp