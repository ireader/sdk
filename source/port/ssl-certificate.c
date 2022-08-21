#if defined(__OPENSSL__)
#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>

X509* openssl_generate_certificate(EVP_PKEY* pkey, const char* common_name)
{
    // webrtc/rtc_base/openssl_certificate.cc
    BIGNUM* rnd = BN_new();
    X509* x509 = X509_new();
    X509_NAME* name = X509_NAME_new();
    if (!x509 || !rnd || !name)
        goto X509_FAILED;

    if(!X509_set_version(x509, 2L) // Set version 3 (note that 0 means version 1).
        || !X509_set_pubkey(x509, pkey))
        goto X509_FAILED;

    // Set serial number (avoid default 0).
    if (!BN_pseudo_rand(rnd, 64, 0, 0) || !BN_to_ASN1_INTEGER(rnd, X509_get_serialNumber(x509)))
        goto X509_FAILED;

    // Set certificate fields.
    if (!X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_UTF8, (unsigned char*)common_name, -1, -1, 0)
        || !X509_NAME_add_entry_by_NID(name, NID_organizationName, MBSTRING_UTF8, (unsigned char*)common_name, -1, -1, 0)
        || !X509_set_subject_name(x509, name)
        || !X509_set_issuer_name(x509, name)
        || !X509_gmtime_adj(X509_get_notBefore(x509), -31536000) // Set valid period, [-1, 1] years
        || !X509_gmtime_adj(X509_get_notAfter(x509), 31536000))
    {
        goto X509_FAILED;
    }

    if (!X509_sign(x509, pkey, EVP_sha256()))
        goto X509_FAILED;

    return x509;

X509_FAILED:
    X509_NAME_free(name);
    X509_free(x509);
    BN_free(rnd);
    return NULL;
}

enum { DTLS_KEY_ECDSA = 1, DTLS_KEY_RSA };

EVP_PKEY* openssl_generate_private_key(int keytype)
{
    // webrtc/rtc_base/openssl_key_pair.cc
    EVP_PKEY* pkey = EVP_PKEY_new();

    if (DTLS_KEY_ECDSA == keytype)
    {
        EC_KEY* eckey = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
        if (!eckey)
        {
            EVP_PKEY_free(pkey);
            return NULL;
        }

        EC_KEY_set_asn1_flag(eckey, OPENSSL_EC_NAMED_CURVE);
        if (!pkey || !eckey || !EC_KEY_generate_key(eckey) || !EVP_PKEY_assign_EC_KEY(pkey, eckey))
        {
            EVP_PKEY_free(pkey);
            EC_KEY_free(eckey);
            return NULL;
        }
    }
    else if (DTLS_KEY_RSA == keytype)
    {
        int bits = 2048; // fixme
        unsigned long long pub_exp = 0;  // fixme
        RSA* rsa = RSA_new();
        BIGNUM* exponent = BN_new();
        if (!pkey || !rsa || !exponent || !BN_set_word(exponent, pub_exp)
            || !RSA_generate_key_ex(rsa, bits, exponent, NULL)
            || !EVP_PKEY_assign_RSA(pkey, rsa))
        {
            EVP_PKEY_free(pkey);
            RSA_free(rsa);
            BN_free(exponent);
            return NULL;
        }
    }
    else
    {
        assert(0);
        EVP_PKEY_free(pkey);
        return NULL;
    }

    return pkey;
}

/// @param[in] algorithm 0-SHA1, 1-SHA224, 2-SHA256, 3-SHA384, 4-SHA512
int openssl_certificate_fingerprint(const X509* x509, int algorithm, char* fingerprint, int bytes)
{
    uint8_t binaryFingerprint[EVP_MAX_MD_SIZE];
    unsigned int n = sizeof(binaryFingerprint);
    unsigned int i;

    const EVP_MD* hashFunction;
    switch (algorithm)
    {
    case 0:
        hashFunction = EVP_sha1();
        break;

    case 1:
        hashFunction = EVP_sha224();
        break;

    case 2:
        hashFunction = EVP_sha256();
        break;

    case 3:
        hashFunction = EVP_sha384();
        break;

    case 4:
        hashFunction = EVP_sha512();
        break;

    default:
        assert(0);
        return -1;
    }

    if (0 == X509_digest(x509, hashFunction, binaryFingerprint, &n))
        return -1;

    if (bytes < (int)(n * 3) || n < 1)
        return -E2BIG;

    for (i = 0; i < n; i++)
    {
        snprintf(fingerprint + i * 3, 4, "%.2X:", (unsigned int)binaryFingerprint[i]);
    }

    fingerprint[i * 3 - 1] = '\0';
    return 0;
}

#endif /* __OPENSSL__ */
