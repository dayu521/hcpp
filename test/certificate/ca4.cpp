#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <iostream>
#include <string>

void generate_key_id(const EVP_PKEY* pkey, unsigned char* key_id, size_t key_id_len) {

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

bool generateCACertificate(const std::string& caKeyPath, const std::string& caCertPath, int days) {
  // Generate a new RSA key pair for the CA.
  EVP_PKEY* caKey = EVP_PKEY_new();
  EVP_PKEY_CTX* caKeyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  EVP_PKEY_keygen_init(caKeyCtx);
  EVP_PKEY_CTX_set_rsa_keygen_bits(caKeyCtx, 2048);
  EVP_PKEY_keygen(caKeyCtx, &caKey);
  EVP_PKEY_CTX_free(caKeyCtx);

  // Create a new X509 certificate for the CA.
  X509* caCert = X509_new();
  X509_set_version(caCert, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(caCert), 1);
  X509_gmtime_adj(X509_get_notBefore(caCert), 0);
  X509_gmtime_adj(X509_get_notAfter(caCert), days * 24 * 60 * 60);
  X509_set_pubkey(caCert, caKey);

  // Set the subject name for the CA certificate.
  X509_NAME* caName = X509_NAME_new();
  X509_NAME_add_entry_by_txt(caName, "CN", MBSTRING_ASC, (const unsigned char*)"My CA", -1, -1, 0);
  X509_set_subject_name(caCert, caName);
  X509_set_issuer_name(caCert, caName);
  X509_NAME_free(caName);

  // Add the basic constraints extension to the CA certificate.
  auto e1=X509V3_EXT_conf_nid(nullptr, nullptr, NID_basic_constraints, "CA:true");
  X509_add_ext(caCert, e1, -1);

  // Add the key usage extension to the CA certificate.
  auto e2=X509V3_EXT_conf_nid(nullptr, nullptr,NID_key_usage, "keyCertSign, cRLSign");
  X509_add_ext(caCert, e2, -1);

  EVP_PKEY* pkey = caKey; // Get the public key
  unsigned char key_id[20];  // 20 bytes for SHA-1
  generate_key_id(pkey, key_id, sizeof(key_id));

  auto oct=ASN1_OCTET_STRING_new();
  ASN1_OCTET_STRING_set(oct, key_id, sizeof(key_id));

  if(X509_add1_ext_i2d(caCert, NID_subject_key_identifier, oct, 0, X509V3_ADD_DEFAULT)==0){
    std::cout<<"添加subject key identifier extension失败"<<std::endl;
    return false;
  }
  auto oct2=ASN1_OCTET_STRING_dup(oct);
  auto akid= AUTHORITY_KEYID_new();
  akid->keyid=oct2;
  X509_add1_ext_i2d(caCert, NID_authority_key_identifier, akid, 0, X509V3_ADD_DEFAULT);
  ASN1_OCTET_STRING_free(oct);

  // Add the subject key identifier extension to the CA certificate.
//   auto e3=X509V3_EXT_conf_nid(nullptr, nullptr,NID_subject_key_identifier, "hash");
//   X509_add_ext(caCert, e3, -1);
    // GENERAL_NAMES *gens = sk_GENERAL_NAME_new_null();

    // std::string dns_name = "www.example.com";
    // GENERAL_NAME *gen_dns = GENERAL_NAME_new();
    // ASN1_IA5STRING *ia5 = ASN1_IA5STRING_new();
    // ASN1_STRING_set(ia5, dns_name.data(), dns_name.length());
    // GENERAL_NAME_set0_value(gen_dns, GEN_DNS, ia5);
    // sk_GENERAL_NAME_push(gens, gen_dns);


  // Add the authority key identifier extension to the CA certificate.
  // auto e4=X509V3_EXT_conf_nid(nullptr, nullptr,NID_authority_key_identifier, "keyid:always,issuer:always");
  // X509_add_ext(caCert, e4, -1);

  // Sign the CA certificate with its private key.
  X509_sign(caCert, caKey, EVP_sha256());

  // Save the CA private key to a file.
  FILE* caKeyFile = fopen(caKeyPath.c_str(), "wb");
  if (!caKeyFile) {
    EVP_PKEY_free(caKey);
    X509_free(caCert);
    return false;
  }
  PEM_write_PrivateKey(caKeyFile, caKey, nullptr, nullptr, 0, nullptr, nullptr);
  fclose(caKeyFile);

  // Save the CA certificate to a file.
  FILE* caCertFile = fopen(caCertPath.c_str(), "wb");
  if (!caCertFile) {
    EVP_PKEY_free(caKey);
    X509_free(caCert);
    return false;
  }
  PEM_write_X509(caCertFile, caCert);
  fclose(caCertFile);

  EVP_PKEY_free(caKey);
  X509_free(caCert);
  return true;
}

int main() {
  std::string caKeyPath = "ca_key.pem";
  std::string caCertPath = "ca_cert.pem";
  int days = 365;

  if (generateCACertificate(caKeyPath, caCertPath, days)) {
    std::cout << "CA certificate generated successfully.\n";
  } else {
    std::cout << "Failed to generate CA certificate.\n";
  }

  return 0;
}