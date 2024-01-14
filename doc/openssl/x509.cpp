#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <string>

// key usage
// https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.3
void add_key_usage(X509 *caCert)
{
    auto bb = ASN1_BIT_STRING_new();
    ASN1_BIT_STRING_set_bit(bb, KU_CRL_SIGN, 1);
    X509_add1_ext_i2d(caCert, NID_key_usage, bb, 0, X509_ADD_FLAG_DEFAULT);
    ASN1_BIT_STRING_free(bb);
}

// Subject Key Identifier
// https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.2
void add_SKI(X509 *caCert)
{
    // TODO 生成key_id
    auto oct = ASN1_OCTET_STRING_new();
    ASN1_OCTET_STRING_set(oct, key_id, sizeof(key_id));
    X509_add1_ext_i2d(caCert, NID_subject_key_identifier, oct, 0, X509V3_ADD_DEFAULT);
    ASN1_OCTET_STRING_free(oct);
}

// Authority Key Identifier
// https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.1
void add_AKI(X509 *caCert)
{
    auto oct = ASN1_OCTET_STRING_new();
    // TODO 初始化oct
    auto akid = AUTHORITY_KEYID_new();
    akid->keyid = oct;
    akid->issuer = names;
    akid->serial = serial;
    X509_add1_ext_i2d(caCert, NID_authority_key_identifier, akid, 0, X509V3_ADD_DEFAULT);
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

    in_addr_t ipv4 = inet_addr("10.0.0.1");
    GENERAL_NAME *gen_ip = GENERAL_NAME_new();
    ASN1_OCTET_STRING *octet = ASN1_OCTET_STRING_new();
    ASN1_STRING_set(octet, &ipv4, sizeof(ipv4));
    GENERAL_NAME_set0_value(gen_ip, GEN_IPADD, octet);
    sk_GENERAL_NAME_push(gens, gen_ip);

    X509_add1_ext_i2d(caCert, NID_subject_alt_name, gens, 0, X509V3_ADD_DEFAULT);

    sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
}

// Basic Constraints
// https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.9
void add_BS(X509 *caCert)
{
    auto bs = BASIC_CONSTRAINTS_new();
    bs->ca = ASN1_BOOLEAN(true);
    X509_add1_ext_i2d(caCert, NID_basic_constraints, bs, 0, X509V3_ADD_DEFAULT);
    ASN1_OCTET_STRING_free(oct);
}

//

void add_subject_name()
{
    // X509_set_subject_name 	get X509_NAME hashes or get and set issuer or subject names
}

// X509_new
// X509_sign
// X509_set_pubkey 	get or set certificate or certificate request public key
// X509_set_serialNumber 	get or set certificate serial number
// X509_set_version
// X509_set_issuer_name