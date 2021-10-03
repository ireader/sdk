#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include <assert.h>
#include <stdio.h>

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
