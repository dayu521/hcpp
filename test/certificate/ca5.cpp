#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include <string>
#include <cstring>

#include <doctest/doctest.h>

// X509_new

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
void add_issuer(X509 *cert)
{
    // auto issuer = X509_NAME_new();
    //   * country,
    //   * organization,
    //   * organizational unit,
    //   x distinguished name qualifier,
    //   * state or province name,
    //   * common name (e.g., "Susan Housley"), and
    //   x serial number.

    //   * locality
    struct name_entry
    {
        int nid_;
        const char *nid_val_;
        int nid_val_len_;
    };
    name_entry issuer[] = {
        {NID_countryName, "CN", sizeof("CN") - 1},
        {NID_organizationName, "NoBody", sizeof("NoBody") - 1},
        {NID_organizationalUnitName, "XX", sizeof("XX") - 1},
        {NID_stateOrProvinceName, "JS", sizeof("JS") - 1},
        {NID_commonName, "SelfCA", sizeof("SelfCA") - 1},
        {NID_localityName, "SQ", sizeof("SQ") - 1},
    };

    auto name = X509_NAME_new();

    for (auto &&i : issuer)
    {
        X509_NAME_add_entry_by_NID(name, i.nid_, MBSTRING_ASC, (const unsigned char *)i.nid_val_, i.nid_val_len_, -1, 0);
    }
    // X509_set_issuer_name
    X509_set_issuer_name(cert, name);
    X509_NAME_free(name);
}

// Validity
// https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.5
void set_validity(X509 *cert)
{
    // 这两个时间点都是包含的
    auto begin_time = ASN1_TIME_new();
    auto end_time = ASN1_TIME_new();
    // now
    X509_time_adj_ex(begin_time, 0, 0, nullptr);
    X509_time_adj_ex(end_time, 365 * 10, 0, nullptr);

    X509_set1_notBefore(cert, begin_time);
    X509_set1_notAfter(cert, end_time);
}

// Subject
// https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.6
void set_subject(X509 *cert)
{
    // X509_set_subject_name 	get X509_NAME hashes or get and set issuer or subject names
    X509_set_subject_name(cert, X509_get_issuer_name(cert));
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
void add_key_usage(X509 *caCert)
{
    auto bb = ASN1_BIT_STRING_new();
    // ASN1_BIT_STRING_set_bit(bb, 6, 1);
    // ASN1_BIT_STRING_set_bit(bb, 5, 1);
    char8_t bv = KU_KEY_CERT_SIGN | KU_CRL_SIGN;
    ASN1_BIT_STRING_set(bb, (unsigned char *)&bv, sizeof(bv));
    X509_add1_ext_i2d(caCert, NID_key_usage, bb, 1, X509_ADD_FLAG_DEFAULT);
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
void add_SAN(X509 *caCert)
{
    GENERAL_NAMES *gens = sk_GENERAL_NAME_new_null();

    std::string dns_name = "www.example.com";
    GENERAL_NAME *gen_dns = GENERAL_NAME_new();
    ASN1_IA5STRING *ia5 = ASN1_IA5STRING_new();
    ASN1_STRING_set(ia5, dns_name.data(), dns_name.length());
    GENERAL_NAME_set0_value(gen_dns, GEN_DNS, ia5);
    sk_GENERAL_NAME_push(gens, gen_dns);

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
void add_BS(X509 *cert)
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

void print_key(EVP_PKEY *pkey);

void print_cert(X509 *cert)
{
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(bio, cert);
    auto cert_size = BIO_pending(bio);
    auto cert_array = new char8_t[cert_size + 1];
    BIO_read(bio, cert_array, cert_size);
    BIO_free_all(bio);

    for (size_t i = 0; i < cert_size; i++)
    {
        printf("%c", cert_array[i]);
    }
}

void print_key(EVP_PKEY *pkey)
{
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL);
    auto pkey_size = BIO_pending(bio);
    auto pkey_array = new char8_t[pkey_size + 1];
    BIO_read(bio, pkey_array, pkey_size);
    BIO_free_all(bio);
    for (size_t i = 0; i < pkey_size; i++)
    {
        printf("%c", pkey_array[i]);
    }
}

TEST_CASE("example")
{

    auto cert = X509_new();

    auto pkey = generate_pkey();
    if (pkey == NULL)
        CHECK(false);
    // print_key(pkey);

    set_version(cert);
    set_validity(cert);
    set_serialNumber(cert);
    add_issuer(cert);
    set_subject(cert);
    // add_SAN(cert);
    add_BS(cert);
    add_AKI(cert);
    add_SKI(cert);
    add_key_usage(cert);
    set_pubkey(cert, pkey);
    sign(cert, pkey);

    // print_cert(cert);

    // Save the private key to a file.
    FILE *privateKeyFile = fopen("hcpp.key.pem", "w");
    if (!privateKeyFile)
    {
        CHECK(false);
    }
    if (!PEM_write_PrivateKey(privateKeyFile, pkey, nullptr, nullptr, 0, nullptr, nullptr))
    {
        fclose(privateKeyFile);
        CHECK(false);
    }
    fclose(privateKeyFile);

    // Save the CA certificate to a file.
    // auto certfile="cert";

    FILE *caCertFile = fopen("hcpp.crt.pem", "wb");
    if (!caCertFile)
    {
        X509_free(cert);
        CHECK(false);
    }
    PEM_write_X509(caCertFile, cert);

    fclose(caCertFile);
    X509_free(cert);
}