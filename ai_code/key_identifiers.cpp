#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <openssl/x509v3.h>

// 把生成的 keyid添加到证书扩展中
void add_AUTHORITY_KEYID() {
    // Create a new authority key identifier
    AUTHORITY_KEYID* akid = AUTHORITY_KEYID_new();

    // Set the key identifier (optional)
    unsigned char keyid_value[] = {0x01, 0x02, 0x03, 0x04};  // Replace with actual key identifier value
    akid->keyid = ASN1_OCTET_STRING_new();
    ASN1_OCTET_STRING_set(akid->keyid, keyid_value, sizeof(keyid_value));

    // Set the authority cert issuer and serial number (optional)
    akid->issuer = /* Set the X509_NAME of the issuer */;
    akid->serial = /* Set the ASN1_INTEGER of the serial number */;

    // Add the authority key identifier extension to the certificate
    X509_add1_ext_i2d(cert, NID_authority_key_identifier, akid, 0, X509V3_ADD_REPLACE);

    // Clean up memory
    AUTHORITY_KEYID_free(akid);

    /* Other certificate processing and cleanup */
}

#include <openssl/evp.h>
#include <openssl/rsa.h>

int get_public_key() {
    // Assume you have an existing EVP_PKEY object, named pkey

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(pkey, NULL);
    if (ctx == NULL) {
        // Handle error
    }

    if (EVP_PKEY_public_check(ctx) <= 0) {
        // Handle error - not a public key
    }

    EVP_PKEY *public_key = NULL;
    if (EVP_PKEY_keygen_init(ctx) <= 0 || EVP_PKEY_keygen(ctx, &public_key) <= 0) {
        // Handle error
    }

    // Now you can use the public_key as the public key

    // Clean up
    EVP_PKEY_free(pkey);       // Free the original EVP_PKEY object
    EVP_PKEY_CTX_free(ctx);    // Free the context
    EVP_PKEY_free(public_key);  // Free the public_key object

    return 0;
}

// 生成方式https://www.rfc-editor.org/rfc/rfc5280#section-4.2.1.2
// 通过public key生成keyid
void generate_key_id(const EVP_PKEY* pkey, unsigned char* key_id, size_t key_id_len) {
    EVP_PKEY_expor
    // Get the public key
    const RSA* rsa_key = EVP_PKEY_get0_RSA(pkey);

    // Get the public modulus
    const BIGNUM* modulus = RSA_get0_n(rsa_key);

    // Convert the modulus to a byte array
    int modulus_len = BN_num_bytes(modulus);
    unsigned char* modulus_bytes = new unsigned char[modulus_len];
    BN_bn2bin(modulus, modulus_bytes);

    // Compute the SHA-1 hash of the modulus
    unsigned char sha1_hash[SHA_DIGEST_LENGTH];
    SHA1(modulus_bytes, modulus_len, sha1_hash);

    // Copy the first key_id_len bytes of the SHA-1 hash to the key ID
    memcpy(key_id, sha1_hash, key_id_len);

    // Clean up memory
    delete[] modulus_bytes;
}

#include <openssl/evp.h>

void createSubjectKeyIdentifier(X509_PUBKEY *pubkey) {
    unsigned char *digest;
    int digest_len;
    X509_PUBKEY_get0_param(NULL, NULL, NULL, &digest, &digest_len, pubkey);

    // Step 2: Compute the hash of the public key
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    EVP_DigestInit(mdctx, EVP_sha1()); // You may choose a different hash algorithm
    EVP_DigestUpdate(mdctx, digest, digest_len);
    EVP_DigestFinal(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    // Step 3: Convert the hash to a Subject Key Identifier format
    ASN1_OCTET_STRING *asn1_skid = ASN1_OCTET_STRING_new();
    ASN1_OCTET_STRING_set(asn1_skid, hash, hash_len);

    // Now 'asn1_skid' contains the Subject Key Identifier
    // You can use it as needed, for example, to set it in the X509 extension.
    // ...
}