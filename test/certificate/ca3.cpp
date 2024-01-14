//HACK 来自 https://codepal.ai/code-generator/query/uKKwCqAF/generate-certificate-from-openssl

// Including the necessary libraries for OpenSSL and file operations.
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <fstream>
#include <iostream>

/**
* Generates a self-signed certificate using OpenSSL.
*
* @param commonName The common name for the certificate.
* @param isCA A boolean indicating whether the certificate should be a CA certificate.
* @param privateKeyPath The file path to save the private key.
* @param certificatePath The file path to save the certificate.
* @return bool True if the certificate generation is successful, false otherwise.
*/
bool generateCertificate(const std::string& commonName, bool isCA, const std::string& privateKeyPath, const std::string& certificatePath) {
    // Initialize OpenSSL.
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    // Generate a new RSA key pair.
    RSA* rsaKeyPair = RSA_generate_key(2048, RSA_F4, nullptr, nullptr);
    if (!rsaKeyPair) {
        std::cerr << "Failed to generate RSA key pair." << std::endl;
        return false;
    }

    // Create a new X509 certificate.
    X509* certificate = X509_new();
    if (!certificate) {
        std::cerr << "Failed to create X509 certificate." << std::endl;
        RSA_free(rsaKeyPair);
        return false;
    }

    // Set the certificate version.
    X509_set_version(certificate, 2);

    // Generate a new serial number for the certificate.
    ASN1_INTEGER* serialNumber = X509_get_serialNumber(certificate);
    if (!serialNumber) {
        std::cerr << "Failed to generate serial number for the certificate." << std::endl;
        X509_free(certificate);
        RSA_free(rsaKeyPair);
        return false;
    }
    ASN1_INTEGER_set(serialNumber, 1);

    // Set the issuer and subject names.
    X509_NAME* name = X509_NAME_new();
    if (!name) {
        std::cerr << "Failed to create X509 name." << std::endl;
        X509_free(certificate);
        RSA_free(rsaKeyPair);
        return false;
    }
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(commonName.c_str()), -1, -1, 0);
    X509_set_issuer_name(certificate, name);
    X509_set_subject_name(certificate, name);

    // Set the certificate's public key.
    EVP_PKEY* publicKey = EVP_PKEY_new();
    if (!publicKey) {
        std::cerr << "Failed to create EVP public key." << std::endl;
        X509_free(certificate);
        RSA_free(rsaKeyPair);
        return false;
    }
    EVP_PKEY_assign_RSA(publicKey, rsaKeyPair);
    X509_set_pubkey(certificate, publicKey);

    // Set the certificate's validity period.
    X509_gmtime_adj(X509_get_notBefore(certificate), 0);
    X509_gmtime_adj(X509_get_notAfter(certificate), 31536000L); // 1 year validity period

    // Set the certificate's extensions.
    X509_EXTENSION* extension = nullptr;
    if (isCA) {
        // Create a basicConstraints extension for CA certificates.
        BASIC_CONSTRAINTS* basicConstraints = BASIC_CONSTRAINTS_new();
        if (!basicConstraints) {
            std::cerr << "Failed to create basicConstraints extension." << std::endl;
            X509_free(certificate);
            EVP_PKEY_free(publicKey);
            return false;
        }
        basicConstraints->ca = 1;
        basicConstraints->pathlen = nullptr;
        X509_add1_ext_i2d(certificate, NID_basic_constraints,basicConstraints,0,X509_ADD_FLAG_DEFAULT);
        BASIC_CONSTRAINTS_free(basicConstraints);
    }
    else {
        // Create a subjectKeyIdentifier extension for non-CA certificates.
        X509_EXTENSION* subjectKeyIdentifier = X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_key_identifier, "hash");
        extension = subjectKeyIdentifier;
    }

    // Add the extension to the certificate.
    if (!X509_add_ext(certificate, extension, -1)) {
        std::cerr << "Failed to add extension to the certificate." << std::endl;
        X509_free(certificate);
        EVP_PKEY_free(publicKey);
        return false;
    }

    // Sign the certificate with the private key.
    if (!X509_sign(certificate, publicKey, EVP_sha256())) {
        std::cerr << "Failed to sign the certificate." << std::endl;
        X509_free(certificate);
        EVP_PKEY_free(publicKey);
        return false;
    }

    // Save the private key to a file.
    FILE* privateKeyFile = fopen(privateKeyPath.c_str(), "w");
    if (!privateKeyFile) {
        std::cerr << "Failed to open private key file for writing." << std::endl;
        X509_free(certificate);
        EVP_PKEY_free(publicKey);
        return false;
    }
    EVP_PKEY *pbkey = EVP_PKEY_new();  
    EVP_PKEY_assign_RSA(pbkey, rsaKeyPair);  
    if (!PEM_write_PrivateKey(privateKeyFile, pbkey, nullptr, nullptr, 0, nullptr, nullptr)) {
        std::cerr << "Failed to write private key to file." << std::endl;
        fclose(privateKeyFile);
        X509_free(certificate);
        EVP_PKEY_free(publicKey);
        return false;
    }
    fclose(privateKeyFile);

    // Save the certificate to a file.
    FILE* certificateFile = fopen(certificatePath.c_str(), "w");
    if (!certificateFile) {
        std::cerr << "Failed to open certificate file for writing." << std::endl;
        X509_free(certificate);
        EVP_PKEY_free(publicKey);
        return false;
    }
    if (!PEM_write_X509(certificateFile, certificate)) {
        std::cerr << "Failed to write certificate to file." << std::endl;
        fclose(certificateFile);
        X509_free(certificate);
        EVP_PKEY_free(publicKey);
        return false;
    }
    fclose(certificateFile);

    // Clean up resources.
    X509_free(certificate);
    EVP_PKEY_free(publicKey);

    return true;
}

int main() {
    // Generate a CA certificate.
    if (generateCertificate("MyCA", true, "ca_private_key.pem", "ca_certificate.pem")) {
        std::cout << "CA certificate generated successfully." << std::endl;
    }
    else {
        std::cerr << "Failed to generate CA certificate." << std::endl;
    }

    // Generate a non-CA certificate.
    if (generateCertificate("MyCertificate", false, "private_key.pem", "certificate.pem")) {
        std::cout << "Non-CA certificate generated successfully." << std::endl;
    }
    else {
        std::cerr << "Failed to generate non-CA certificate." << std::endl;
    }

    return 0;
}