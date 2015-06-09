/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include "intelkeymaster_firmware_api.h"
#include "intelkeymaster_middleware_api.h"

#include <keystore/keystore.h>

#include <hardware/hardware.h>
#include <hardware/keymaster.h>

#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/x509.h>

#include <UniquePtr.h>
#include <cutils/properties.h>

// For debugging
//#define LOG_NDEBUG 0

#define LOG_TAG "IntelKeyMaster"
#include <cutils/log.h>

struct BIGNUM_Delete {
    void operator()(BIGNUM* p) const {
        BN_free(p);
    }
};
typedef UniquePtr<BIGNUM, BIGNUM_Delete> Unique_BIGNUM;

struct EVP_PKEY_Delete {
    void operator()(EVP_PKEY* p) const {
        EVP_PKEY_free(p);
    }
};
typedef UniquePtr<EVP_PKEY, EVP_PKEY_Delete> Unique_EVP_PKEY;

struct PKCS8_PRIV_KEY_INFO_Delete {
    void operator()(PKCS8_PRIV_KEY_INFO* p) const {
        PKCS8_PRIV_KEY_INFO_free(p);
    }
};
typedef UniquePtr<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_Delete> Unique_PKCS8_PRIV_KEY_INFO;

struct DSA_Delete {
    void operator()(DSA* p) const {
        DSA_free(p);
    }
};
typedef UniquePtr<DSA, DSA_Delete> Unique_DSA;

struct EC_KEY_Delete {
    void operator()(EC_KEY* p) const {
        EC_KEY_free(p);
    }
};
typedef UniquePtr<EC_KEY, EC_KEY_Delete> Unique_EC_KEY;

struct RSA_Delete {
    void operator()(RSA* p) const {
        RSA_free(p);
    }
};
typedef UniquePtr<RSA, RSA_Delete> Unique_RSA;

typedef UniquePtr<keymaster_device_t> Unique_keymaster_device_t;

/* intel keymaster functions */
int intel_keymaster_generate_rsa_keypair(const keymaster_rsa_keygen_params_t* rsa_params,
                                         uint8_t** keyBlob, size_t* keyBlobLength);

int intel_keymaster_import_rsa_keypair(const RSA *rsa,
                                       uint8_t** keyBlob, size_t* keyBlobLength);

EVP_PKEY* intel_keymaster_unwrap_key(const uint8_t* keyBlob, const size_t keyBlobLength);

int intel_keymaster_sign_rsa(const uint8_t* keyBlob, const size_t keyBlobLength,
                             const uint8_t* data, const size_t dataLength,
                             uint8_t** signedData, size_t* signedDataLength);

int intel_keymaster_verify_rsa(const uint8_t* keyBlob, const size_t keyBlobLength,
                               const uint8_t* signedData, const size_t signedDataLength,
                               const uint8_t* signature, const size_t signatureLength);

/**
 * Many OpenSSL APIs take ownership of an argument on success but don't free the argument
 * on failure. This means we need to tell our scoped pointers when we've transferred ownership,
 * without triggering a warning by not using the result of release().
 */
#define OWNERSHIP_TRANSFERRED(obj) \
    typeof (obj.release()) _dummy __attribute__((unused)) = obj.release()


/*
 * Checks this thread's OpenSSL error queue and logs if
 * necessary.
 */
static void logOpenSSLError(const char* location) {
    int error = ERR_get_error();

    if (error != 0) {
        char message[256];
        ERR_error_string_n(error, message, sizeof(message));
        ALOGE("OpenSSL error in %s %d: %s", location, error, message);
    }

    ERR_clear_error();
    ERR_remove_state(0);
}

static int wrap_key(EVP_PKEY* pkey, int type, uint8_t** keyBlob, size_t* keyBlobLength) {
    /*
     *  Find the length of each size. Public key is not needed anymore but must be kept for
     * alignment purposes.
     */
    int publicLen = 0;
    int privateLen = i2d_PrivateKey(pkey, NULL);

    if (privateLen <= 0) {
        ALOGE("private key size was too big");
        return -1;
    }

    /* int type + int size + private key data + int size + public key data */
    *keyBlobLength = get_softkey_header_size() + sizeof(int) + sizeof(int) + privateLen
            + sizeof(int) + publicLen;

    UniquePtr<unsigned char> derData(new unsigned char[*keyBlobLength]);
    if (derData.get() == NULL) {
        ALOGE("could not allocate memory for key blob");
        return -1;
    }
    unsigned char* p = derData.get();

    /* Write the magic value for software keys. */
    p = add_softkey_header(p, *keyBlobLength);

    /* Write key type to allocated buffer */
    for (int i = sizeof(int) - 1; i >= 0; i--) {
        *p++ = (type >> (8*i)) & 0xFF;
    }

    /* Write public key to allocated buffer */
    for (int i = sizeof(int) - 1; i >= 0; i--) {
        *p++ = (publicLen >> (8*i)) & 0xFF;
    }

    /* Write private key to allocated buffer */
    for (int i = sizeof(int) - 1; i >= 0; i--) {
        *p++ = (privateLen >> (8*i)) & 0xFF;
    }
    if (i2d_PrivateKey(pkey, &p) != privateLen) {
        logOpenSSLError("wrap_key");
        return -1;
    }

    *keyBlob = derData.release();

    return 0;
}

static EVP_PKEY* unwrap_key(const uint8_t* keyBlob, const size_t keyBlobLength) {
    long publicLen = 0;
    long privateLen = 0;
    const uint8_t* p = keyBlob;
    const uint8_t *const end = keyBlob + keyBlobLength;

    if (keyBlob == NULL) {
        ALOGE("supplied key blob was NULL");
        return NULL;
    }

    // Should be large enough for:
    // int32 magic, int32 type, int32 pubLen, char* pub, int32 privLen, char* priv
    if (keyBlobLength < (get_softkey_header_size() + sizeof(int) + sizeof(int) + 1
            + sizeof(int) + 1)) {
        ALOGE("key blob appears to be truncated");
        return NULL;
    }

    if (!is_softkey(p, keyBlobLength)) {
        ALOGE("cannot read key; it was not made by this keymaster");
        return NULL;
    }
    p += get_softkey_header_size();

    int type = 0;
    for (size_t i = 0; i < sizeof(int); i++) {
        type = (type << 8) | *p++;
    }

    for (size_t i = 0; i < sizeof(int); i++) {
        publicLen = (publicLen << 8) | *p++;
    }
    if (p + publicLen > end) {
        ALOGE("public key length encoding error: size=%ld, end=%d", publicLen, end - p);
        return NULL;
    }

    p += publicLen;
    if (end - p < 2) {
        ALOGE("private key truncated");
        return NULL;
    }
    for (size_t i = 0; i < sizeof(int); i++) {
        privateLen = (privateLen << 8) | *p++;
    }
    if (p + privateLen > end) {
        ALOGE("private key length encoding error: size=%ld, end=%d", privateLen, end - p);
        return NULL;
    }

    Unique_EVP_PKEY pkey(EVP_PKEY_new());
    if (pkey.get() == NULL) {
        logOpenSSLError("unwrap_key");
        return NULL;
    }
    EVP_PKEY* tmp = pkey.get();

    if (d2i_PrivateKey(type, &tmp, &p, privateLen) == NULL) {
        logOpenSSLError("unwrap_key");
        return NULL;
    }

    return pkey.release();
}

static int generate_dsa_keypair(EVP_PKEY* pkey, const keymaster_dsa_keygen_params_t* dsa_params)
{
    if (dsa_params->key_size < 512) {
        ALOGI("Requested DSA key size is too small (<512)");
        return -1;
    }

    Unique_DSA dsa(DSA_new());

    if (dsa.get() == NULL) {
        return -1;
    }

    if (dsa_params->generator_len == 0 ||
            dsa_params->prime_p_len == 0 ||
            dsa_params->prime_q_len == 0 ||
            dsa_params->generator == NULL||
            dsa_params->prime_p == NULL ||
            dsa_params->prime_q == NULL) {
        if (DSA_generate_parameters_ex(dsa.get(), dsa_params->key_size, NULL, 0, NULL, NULL,
                NULL) != 1) {
            logOpenSSLError("generate_dsa_keypair");
            return -1;
        }
    } else {
        dsa->g = BN_bin2bn(dsa_params->generator,
                dsa_params->generator_len,
                NULL);
        if (dsa->g == NULL) {
            logOpenSSLError("generate_dsa_keypair");
            return -1;
        }

        dsa->p = BN_bin2bn(dsa_params->prime_p,
                   dsa_params->prime_p_len,
                   NULL);
        if (dsa->p == NULL) {
            logOpenSSLError("generate_dsa_keypair");
            return -1;
        }

        dsa->q = BN_bin2bn(dsa_params->prime_q,
                   dsa_params->prime_q_len,
                   NULL);
        if (dsa->q == NULL) {
            logOpenSSLError("generate_dsa_keypair");
            return -1;
        }
    }

    if (DSA_generate_key(dsa.get()) != 1) {
        logOpenSSLError("generate_dsa_keypair");
        return -1;
    }

    if (EVP_PKEY_assign_DSA(pkey, dsa.get()) == 0) {
        logOpenSSLError("generate_dsa_keypair");
        return -1;
    }
    OWNERSHIP_TRANSFERRED(dsa);

    return 0;
}

static int generate_ec_keypair(EVP_PKEY* pkey, const keymaster_ec_keygen_params_t* ec_params)
{
    EC_GROUP* group;
    switch (ec_params->field_size) {
    case 192:
        group = EC_GROUP_new_by_curve_name(NID_X9_62_prime192v1);
        break;
    case 224:
        group = EC_GROUP_new_by_curve_name(NID_secp224r1);
        break;
    case 256:
        group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
        break;
    case 384:
        group = EC_GROUP_new_by_curve_name(NID_secp384r1);
        break;
    case 521:
        group = EC_GROUP_new_by_curve_name(NID_secp521r1);
        break;
    default:
        group = NULL;
        break;
    }

    if (group == NULL) {
        logOpenSSLError("generate_ec_keypair");
        return -1;
    }

    EC_GROUP_set_point_conversion_form(group, POINT_CONVERSION_UNCOMPRESSED);
    EC_GROUP_set_asn1_flag(group, OPENSSL_EC_NAMED_CURVE);

    /* initialize EC key */
    Unique_EC_KEY eckey(EC_KEY_new());
    if (eckey.get() == NULL) {
        logOpenSSLError("generate_ec_keypair");
        return -1;
    }

    if (EC_KEY_set_group(eckey.get(), group) != 1) {
        logOpenSSLError("generate_ec_keypair");
        return -1;
    }

    if (EC_KEY_generate_key(eckey.get()) != 1
            || EC_KEY_check_key(eckey.get()) < 0) {
        logOpenSSLError("generate_ec_keypair");
        return -1;
    }

    if (EVP_PKEY_assign_EC_KEY(pkey, eckey.get()) == 0) {
        logOpenSSLError("generate_ec_keypair");
        return -1;
    }
    OWNERSHIP_TRANSFERRED(eckey);

    return 0;
}

__attribute__ ((visibility ("default")))
int intel_keymaster_generate_keypair(const keymaster_device_t*,
        const keymaster_keypair_t key_type, const void* key_params,
        uint8_t** keyBlob, size_t* keyBlobLength) {

    if (key_type == TYPE_RSA) {
        return intel_keymaster_generate_rsa_keypair(
           (const keymaster_rsa_keygen_params_t*)key_params,
           keyBlob, keyBlobLength);
    }

    Unique_EVP_PKEY pkey(EVP_PKEY_new());
    if (pkey.get() == NULL) {
        logOpenSSLError("intel_keymaster_generate_keypair");
        return -1;
    }

    if (key_params == NULL) {
        ALOGW("key_params == null");
        return -1;
    } else if (key_type == TYPE_DSA) {
        const keymaster_dsa_keygen_params_t* dsa_params =
                (const keymaster_dsa_keygen_params_t*) key_params;
        generate_dsa_keypair(pkey.get(), dsa_params);
    } else if (key_type == TYPE_EC) {
        const keymaster_ec_keygen_params_t* ec_params =
                (const keymaster_ec_keygen_params_t*) key_params;
        generate_ec_keypair(pkey.get(), ec_params);
    } else {
        ALOGW("Unsupported key type %d", key_type);
        return -1;
    }

    if (wrap_key(pkey.get(), EVP_PKEY_type(pkey->type), keyBlob, keyBlobLength)) {
        return -1;
    }

    return 0;
}

__attribute__ ((visibility ("default")))
int intel_keymaster_import_keypair(const keymaster_device_t*,
        const uint8_t* key, const size_t key_length,
        uint8_t** key_blob, size_t* key_blob_length) {
    if (key == NULL) {
        ALOGW("input key == NULL");
        return -1;
    } else if (key_blob == NULL || key_blob_length == NULL) {
        ALOGW("output key blob or length == NULL");
        return -1;
    }

    Unique_PKCS8_PRIV_KEY_INFO pkcs8(d2i_PKCS8_PRIV_KEY_INFO(NULL, &key, key_length));
    if (pkcs8.get() == NULL) {
        logOpenSSLError("intel_keymaster_import_keypair");
        return -1;
    }

    /* assign to EVP */
    Unique_EVP_PKEY pkey(EVP_PKCS82PKEY(pkcs8.get()));
    if (pkey.get() == NULL) {
        logOpenSSLError("intel_keymaster_import_keypair");
        return -1;
    }
    OWNERSHIP_TRANSFERRED(pkcs8);

    int type = EVP_PKEY_type(pkey->type);
    if (type == EVP_PKEY_RSA) {
        RSA *rsa = pkey->pkey.rsa;
        if (rsa == NULL) {
            ALOGE("rsa == NULL");
            return -1;
        }
        return intel_keymaster_import_rsa_keypair(rsa, key_blob, key_blob_length);
    }

    if (wrap_key(pkey.get(), EVP_PKEY_type(pkey->type), key_blob, key_blob_length)) {
        return -1;
    }

    return 0;
}

__attribute__ ((visibility ("default")))
int intel_keymaster_get_keypair_public(const struct keymaster_device*,
        const uint8_t* key_blob, const size_t key_blob_length,
        uint8_t** x509_data, size_t* x509_data_length) {

    if (x509_data == NULL || x509_data_length == NULL) {
        ALOGW("output public key buffer == NULL");
        return -1;
    }

    Unique_EVP_PKEY pkey(unwrap_key(key_blob, key_blob_length));
    if (pkey.get() == NULL) {
        /* if the keyblob cannot be unwrapped using the softkey method,
           then assume it is intel generated keyblob
           */
        pkey.reset(intel_keymaster_unwrap_key(key_blob, key_blob_length));
        if (pkey.get() == NULL) {
            return -1;
        }
    }

    int len = i2d_PUBKEY(pkey.get(), NULL);
    if (len <= 0) {
        logOpenSSLError("intel_keymaster_get_keypair_public");
        return -1;
    }

    UniquePtr<uint8_t> key(static_cast<uint8_t*>(malloc(len)));
    if (key.get() == NULL) {
        ALOGE("Could not allocate memory for public key data");
        return -1;
    }

    unsigned char* tmp = reinterpret_cast<unsigned char*>(key.get());
    if (i2d_PUBKEY(pkey.get(), &tmp) != len) {
        logOpenSSLError("intel_keymaster_get_keypair_public");
        return -1;
    }

    ALOGV("Length of x509 data is %d", len);
    *x509_data_length = len;
    *x509_data = key.release();

    return 0;
}

static int sign_dsa(EVP_PKEY* pkey, keymaster_dsa_sign_params_t* sign_params, const uint8_t* data,
        const size_t dataLength, uint8_t** signedData, size_t* signedDataLength) {
    if (sign_params->digest_type != DIGEST_NONE) {
        ALOGW("Cannot handle digest type %d", sign_params->digest_type);
        return -1;
    }

    Unique_DSA dsa(EVP_PKEY_get1_DSA(pkey));
    if (dsa.get() == NULL) {
        logOpenSSLError("openssl_sign_dsa");
        return -1;
    }

    unsigned int dsaSize = DSA_size(dsa.get());
    UniquePtr<uint8_t> signedDataPtr(reinterpret_cast<uint8_t*>(malloc(dsaSize)));
    if (signedDataPtr.get() == NULL) {
        logOpenSSLError("openssl_sign_dsa");
        return -1;
    }

    unsigned char* tmp = reinterpret_cast<unsigned char*>(signedDataPtr.get());
    if (DSA_sign(0, data, dataLength, tmp, &dsaSize, dsa.get()) <= 0) {
        logOpenSSLError("openssl_sign_dsa");
        return -1;
    }

    *signedDataLength = dsaSize;
    *signedData = signedDataPtr.release();

    return 0;
}

static int sign_ec(EVP_PKEY* pkey, keymaster_ec_sign_params_t* sign_params, const uint8_t* data,
        const size_t dataLength, uint8_t** signedData, size_t* signedDataLength) {
    if (sign_params->digest_type != DIGEST_NONE) {
        ALOGW("Cannot handle digest type %d", sign_params->digest_type);
        return -1;
    }

    Unique_EC_KEY eckey(EVP_PKEY_get1_EC_KEY(pkey));
    if (eckey.get() == NULL) {
        logOpenSSLError("openssl_sign_ec");
        return -1;
    }

    unsigned int ecdsaSize = ECDSA_size(eckey.get());
    UniquePtr<uint8_t> signedDataPtr(reinterpret_cast<uint8_t*>(malloc(ecdsaSize)));
    if (signedDataPtr.get() == NULL) {
        logOpenSSLError("openssl_sign_ec");
        return -1;
    }

    unsigned char* tmp = reinterpret_cast<unsigned char*>(signedDataPtr.get());
    if (ECDSA_sign(0, data, dataLength, tmp, &ecdsaSize, eckey.get()) <= 0) {
        logOpenSSLError("openssl_sign_ec");
        return -1;
    }

    *signedDataLength = ecdsaSize;
    *signedData = signedDataPtr.release();

    return 0;
}


static int sign_rsa(EVP_PKEY* pkey, keymaster_rsa_sign_params_t* sign_params, const uint8_t* data,
        const size_t dataLength, uint8_t** signedData, size_t* signedDataLength) {
    if (sign_params->digest_type != DIGEST_NONE) {
        ALOGW("Cannot handle digest type %d", sign_params->digest_type);
        return -1;
    } else if (sign_params->padding_type != PADDING_NONE) {
        ALOGW("Cannot handle padding type %d", sign_params->padding_type);
        return -1;
    }

    Unique_RSA rsa(EVP_PKEY_get1_RSA(pkey));
    if (rsa.get() == NULL) {
        logOpenSSLError("openssl_sign_rsa");
        return -1;
    }

    UniquePtr<uint8_t> signedDataPtr(reinterpret_cast<uint8_t*>(malloc(dataLength)));
    if (signedDataPtr.get() == NULL) {
        logOpenSSLError("openssl_sign_rsa");
        return -1;
    }

    unsigned char* tmp = reinterpret_cast<unsigned char*>(signedDataPtr.get());
    if (RSA_private_encrypt(dataLength, data, tmp, rsa.get(), RSA_NO_PADDING) <= 0) {
        logOpenSSLError("openssl_sign_rsa");
        return -1;
    }

    *signedDataLength = dataLength;
    *signedData = signedDataPtr.release();

    return 0;
}

__attribute__ ((visibility ("default")))
int intel_keymaster_sign_data(const keymaster_device_t*,
        const void* params,
        const uint8_t* keyBlob, const size_t keyBlobLength,
        const uint8_t* data, const size_t dataLength,
        uint8_t** signedData, size_t* signedDataLength) {
    if (data == NULL) {
        ALOGW("input data to sign == NULL");
        return -1;
    } else if (signedData == NULL || signedDataLength == NULL) {
        ALOGW("output signature buffer == NULL");
        return -1;
    }

    Unique_EVP_PKEY pkey(unwrap_key(keyBlob, keyBlobLength));
    if (pkey.get() == NULL) {
        /* if the keyblob cannot be unwrapped using the softkey method,
           then assume it is intel generated keyblob
           */
        return intel_keymaster_sign_rsa(keyBlob, keyBlobLength,
                                        data, dataLength,
                                        signedData, signedDataLength);
    }

    int type = EVP_PKEY_type(pkey->type);
    if (type == EVP_PKEY_DSA) {
        keymaster_dsa_sign_params_t* sign_params = (keymaster_dsa_sign_params_t*) params;
        return sign_dsa(pkey.get(), sign_params, data, dataLength, signedData, signedDataLength);
    } else if (type == EVP_PKEY_EC) {
        keymaster_ec_sign_params_t* sign_params = (keymaster_ec_sign_params_t*) params;
        return sign_ec(pkey.get(), sign_params, data, dataLength, signedData, signedDataLength);
    } else if (type == EVP_PKEY_RSA) {
        keymaster_rsa_sign_params_t* sign_params = (keymaster_rsa_sign_params_t*) params;
        return sign_rsa(pkey.get(), sign_params, data, dataLength, signedData, signedDataLength);
    } else {
        ALOGW("Unsupported key type");
        return -1;
    }
}

static int verify_dsa(EVP_PKEY* pkey, keymaster_dsa_sign_params_t* sign_params,
        const uint8_t* signedData, const size_t signedDataLength, const uint8_t* signature,
        const size_t signatureLength) {
    if (sign_params->digest_type != DIGEST_NONE) {
        ALOGW("Cannot handle digest type %d", sign_params->digest_type);
        return -1;
    }

    Unique_DSA dsa(EVP_PKEY_get1_DSA(pkey));
    if (dsa.get() == NULL) {
        logOpenSSLError("openssl_verify_dsa");
        return -1;
    }

    if (DSA_verify(0, signedData, signedDataLength, signature, signatureLength, dsa.get()) <= 0) {
        logOpenSSLError("openssl_verify_dsa");
        return -1;
    }

    return 0;
}

static int verify_ec(EVP_PKEY* pkey, keymaster_ec_sign_params_t* sign_params,
        const uint8_t* signedData, const size_t signedDataLength, const uint8_t* signature,
        const size_t signatureLength) {
    if (sign_params->digest_type != DIGEST_NONE) {
        ALOGW("Cannot handle digest type %d", sign_params->digest_type);
        return -1;
    }

    Unique_EC_KEY eckey(EVP_PKEY_get1_EC_KEY(pkey));
    if (eckey.get() == NULL) {
        logOpenSSLError("openssl_verify_ec");
        return -1;
    }

    if (ECDSA_verify(0, signedData, signedDataLength, signature, signatureLength, eckey.get()) <= 0) {
        logOpenSSLError("openssl_verify_ec");
        return -1;
    }

    return 0;
}

static int verify_rsa(EVP_PKEY* pkey, keymaster_rsa_sign_params_t* sign_params,
        const uint8_t* signedData, const size_t signedDataLength, const uint8_t* signature,
        const size_t signatureLength) {
    if (sign_params->digest_type != DIGEST_NONE) {
        ALOGW("Cannot handle digest type %d", sign_params->digest_type);
        return -1;
    } else if (sign_params->padding_type != PADDING_NONE) {
        ALOGW("Cannot handle padding type %d", sign_params->padding_type);
        return -1;
    } else if (signatureLength != signedDataLength) {
        ALOGW("signed data length must be signature length");
        return -1;
    }

    Unique_RSA rsa(EVP_PKEY_get1_RSA(pkey));
    if (rsa.get() == NULL) {
        logOpenSSLError("verify_rsa");
        return -1;
    }

    UniquePtr<uint8_t> dataPtr(reinterpret_cast<uint8_t*>(malloc(signedDataLength)));
    if (dataPtr.get() == NULL) {
        logOpenSSLError("verify_rsa");
        return -1;
    }

    unsigned char* tmp = reinterpret_cast<unsigned char*>(dataPtr.get());
    if (!RSA_public_decrypt(signatureLength, signature, tmp, rsa.get(), RSA_NO_PADDING)) {
        logOpenSSLError("verify_rsa");
        return -1;
    }

    int result = 0;
    for (size_t i = 0; i < signedDataLength; i++) {
        result |= tmp[i] ^ signedData[i];
    }

    return result == 0 ? 0 : -1;
}

__attribute__ ((visibility ("default")))
int intel_keymaster_verify_data(const keymaster_device_t*,
        const void* params,
        const uint8_t* keyBlob, const size_t keyBlobLength,
        const uint8_t* signedData, const size_t signedDataLength,
        const uint8_t* signature, const size_t signatureLength) {

    if (signedData == NULL || signature == NULL) {
        ALOGW("data or signature buffers == NULL");
        return -1;
    }

    Unique_EVP_PKEY pkey(unwrap_key(keyBlob, keyBlobLength));
    if (pkey.get() == NULL) {
        /* if the keyblob cannot be unwrapped using the softkey method,
           then assume it is intel generated keyblob
           */
        return intel_keymaster_verify_rsa(keyBlob, keyBlobLength,
                                          signedData, signedDataLength,
                                          signature, signatureLength);
    }

    int type = EVP_PKEY_type(pkey->type);
    if (type == EVP_PKEY_DSA) {
        keymaster_dsa_sign_params_t* sign_params = (keymaster_dsa_sign_params_t*) params;
        return verify_dsa(pkey.get(), sign_params, signedData, signedDataLength, signature,
                signatureLength);
    } else if (type == EVP_PKEY_RSA) {
        keymaster_rsa_sign_params_t* sign_params = (keymaster_rsa_sign_params_t*) params;
        return verify_rsa(pkey.get(), sign_params, signedData, signedDataLength, signature,
                signatureLength);
    } else if (type == EVP_PKEY_EC) {
        keymaster_ec_sign_params_t* sign_params = (keymaster_ec_sign_params_t*) params;
        return verify_ec(pkey.get(), sign_params, signedData, signedDataLength, signature,
                signatureLength);
    } else {
        ALOGW("Unsupported key type %d", type);
        return -1;
    }
}

int intel_keymaster_generate_rsa_keypair(const keymaster_rsa_keygen_params_t* rsa_params,
                                         uint8_t** keyBlob, size_t* keyBlobLength)
{
    UniquePtr<intel_keymaster_firmware_cmd_t> cmd_buf(
       static_cast<intel_keymaster_firmware_cmd_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    UniquePtr<intel_keymaster_firmware_rsp_t> rsp_buf(
       static_cast<intel_keymaster_firmware_rsp_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    intel_keymaster_keygen_params_t *keygen_params;
    intel_keymaster_rsa_keygen_params_t *rsa_keygen_params;
    uint32_t cmd_len, rsp_len;
    sep_keymaster_return_t sep_ret;
    intel_keymaster_key_blob_t *key_blob;
    uint32_t rsp_result_and_data_length;

    if ((rsa_params == NULL) || (keyBlob == NULL) || (keyBlobLength == NULL)) {
        return -1;
    }

    if (cmd_buf.get() == NULL) {
        return -1;
    }

    if (rsp_buf.get() == NULL) {
        return -1;
    }

    keygen_params = (intel_keymaster_keygen_params_t *)cmd_buf->cmd_data;
    keygen_params->key_type = KEY_TYPE_RSA;
    rsa_keygen_params = (intel_keymaster_rsa_keygen_params_t *)keygen_params->key_params;
    rsa_keygen_params->modulus_size = rsa_params->modulus_size;
    rsa_keygen_params->reserved = 0;
    rsa_keygen_params->public_exponent = rsa_params->public_exponent;
    cmd_buf->cmd_id = KEYMASTER_CMD_GENERATE_KEYPAIR;
    cmd_buf->cmd_data_length = sizeof(intel_keymaster_rsa_keygen_params_t) +
        offsetof(intel_keymaster_keygen_params_t, key_params);
    cmd_len = cmd_buf->cmd_data_length + offsetof(intel_keymaster_firmware_cmd_t, cmd_data);

    rsp_len = INTEL_KEYMASTER_MAX_MESSAGE_SIZE;
    sep_ret = sep_keymaster_send_cmd((uint8_t *)cmd_buf.get(), cmd_len, (uint8_t *)rsp_buf.get(),
                                     &rsp_len);

    if (sep_ret != SEP_KEYMASTER_SUCCESS) {
        ALOGE("sep_keymaster_send_cmd() returned error %d", sep_ret);
        return -1;
    }

    // response should contain at least rsp_id, rsp_result_and_data_length and result
    if (rsp_len < offsetof(intel_keymaster_firmware_rsp_t, rsp_data)) {
        ALOGE("response is too short, rsp_len = %u", rsp_len);
        return -1;
    }

    // rsp_result_and_data_length should include at least the result field
    if (rsp_buf->rsp_result_and_data_length < sizeof(intel_keymaster_result_t)) {
        ALOGE("rsp_result_and_data_length too small, rsp_result_and_data_length = %u",
              rsp_buf->rsp_result_and_data_length);
        return -1;
    }

    // the result must indicate success
    if (rsp_buf->result != KEYMASTER_RESULT_SUCCESS) {
        ALOGE("firmware returned result is not successful, result = 0x%x", rsp_buf->result);
        return -1;
    }

    key_blob = (intel_keymaster_key_blob_t *)rsp_buf->rsp_data;

    if (key_blob->key_blob_length == 0) {
        ALOGE("key_blob_length is 0");
        return -1;
    }

    rsp_result_and_data_length = key_blob->key_blob_length +
        offsetof(intel_keymaster_key_blob_t, key_blob) + sizeof(intel_keymaster_result_t);
    if (rsp_result_and_data_length != rsp_buf->rsp_result_and_data_length) {
        ALOGE("calculated rsp_result_and_data_length %u does not match rsp_result_and_data_length "
              "%u in structure",
              rsp_result_and_data_length, rsp_buf->rsp_result_and_data_length);
        return -1;
    }

    if (rsp_buf->rsp_result_and_data_length + offsetof(intel_keymaster_firmware_rsp_t, result) !=
        rsp_len) {
        ALOGE("rsp_result_and_data_length %u is inconsistent with rsp_len %u",
              rsp_buf->rsp_result_and_data_length, rsp_len);
        return -1;
    }

    *keyBlobLength = key_blob->key_blob_length;
    /* freeing memory allocated here is the responsibility of the caller several layers above */
    *keyBlob = (uint8_t *)malloc(key_blob->key_blob_length);
    if (*keyBlob == NULL) {
        ALOGE("memory allocation for keyBlob failed");
        return -1;
    }

    memcpy(*keyBlob, key_blob->key_blob, key_blob->key_blob_length);

    return 0;
}

int intel_keymaster_import_rsa_keypair(const RSA *rsa,
                                       uint8_t** keyBlob, size_t* keyBlobLength)
{
    UniquePtr<intel_keymaster_firmware_cmd_t> cmd_buf(
       static_cast<intel_keymaster_firmware_cmd_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    UniquePtr<intel_keymaster_firmware_rsp_t> rsp_buf(
       static_cast<intel_keymaster_firmware_rsp_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    intel_keymaster_import_key_t *import_params;
    intel_keymaster_rsa_key_t *rsa_key;
    uint32_t cmd_len, rsp_len;
    sep_keymaster_return_t sep_ret;
    intel_keymaster_key_blob_t *key_blob;
    uint32_t rsp_result_and_data_length;
    uint32_t modulus_length, public_exponent_length, private_exponent_length;

    if ((rsa == NULL) || (rsa->n == NULL) || (rsa->e == NULL) || (rsa->d == NULL) ||
        (keyBlob == NULL) || (keyBlobLength == NULL)) {
        return -1;
    }

    if (cmd_buf.get() == NULL) {
        return -1;
    }

    if (rsp_buf.get() == NULL) {
        return -1;
    }

    modulus_length = BN_num_bytes(rsa->n);
    public_exponent_length = BN_num_bytes(rsa->e);
    private_exponent_length = BN_num_bytes(rsa->d);

    cmd_buf->cmd_id = KEYMASTER_CMD_IMPORT_KEYPAIR;
    cmd_buf->cmd_data_length = modulus_length + public_exponent_length + private_exponent_length +
        offsetof(intel_keymaster_rsa_key_t, bytes) +
        offsetof(intel_keymaster_import_key_t, key_data);
    cmd_len = cmd_buf->cmd_data_length + offsetof(intel_keymaster_firmware_cmd_t, cmd_data);

    if (cmd_len > INTEL_KEYMASTER_MAX_MESSAGE_SIZE) {
        ALOGE("combination of modulus, public exponent and private exponent length is too big,"
              " modulus_length = %u, public_exponent_length = %u, private_exponent_length = %u",
              modulus_length, public_exponent_length, private_exponent_length);
        return -1;
    }

    import_params = (intel_keymaster_import_key_t *)cmd_buf->cmd_data;
    import_params->key_type = KEY_TYPE_RSA;
    rsa_key = (intel_keymaster_rsa_key_t *)import_params->key_data;
    rsa_key->modulus_length = modulus_length;
    rsa_key->public_exponent_length = public_exponent_length;
    rsa_key->private_exponent_length = private_exponent_length;
    BN_bn2bin(rsa->n, rsa_key->bytes);
    BN_bn2bin(rsa->e, rsa_key->bytes + modulus_length);
    BN_bn2bin(rsa->d, rsa_key->bytes + modulus_length + public_exponent_length);

    rsp_len = INTEL_KEYMASTER_MAX_MESSAGE_SIZE;
    sep_ret = sep_keymaster_send_cmd((uint8_t *)cmd_buf.get(), cmd_len, (uint8_t *)rsp_buf.get(),
                                     &rsp_len);

    if (sep_ret != SEP_KEYMASTER_SUCCESS) {
        ALOGE("sep_keymaster_send_cmd() returned error %d", sep_ret);
        return -1;
    }

    // response should contain at least rsp_id, rsp_result_and_data_length and result
    if (rsp_len < offsetof(intel_keymaster_firmware_rsp_t, rsp_data)) {
        ALOGE("response is too short, rsp_len = %u", rsp_len);
        return -1;
    }

    // rsp_result_and_data_length should include at least the result field
    if (rsp_buf->rsp_result_and_data_length < sizeof(intel_keymaster_result_t)) {
        ALOGE("rsp_result_and_data_length too small, rsp_result_and_data_length = %u",
              rsp_buf->rsp_result_and_data_length);
        return -1;
    }

    // the result must indicate success
    if (rsp_buf->result != KEYMASTER_RESULT_SUCCESS) {
        ALOGE("firmware returned result is not successful, result = 0x%x", rsp_buf->result);
        return -1;
    }

    key_blob = (intel_keymaster_key_blob_t *)rsp_buf->rsp_data;

    if (key_blob->key_blob_length == 0) {
        ALOGE("key_blob_length is 0");
        return -1;
    }

    rsp_result_and_data_length = key_blob->key_blob_length +
        offsetof(intel_keymaster_key_blob_t, key_blob) + sizeof(intel_keymaster_result_t);
    if (rsp_result_and_data_length != rsp_buf->rsp_result_and_data_length) {
        ALOGE("calculated rsp_result_and_data_length %u does not match rsp_result_and_data_length "
              "%u in structure",
              rsp_result_and_data_length, rsp_buf->rsp_result_and_data_length);
        return -1;
    }

    if (rsp_buf->rsp_result_and_data_length + offsetof(intel_keymaster_firmware_rsp_t, result) !=
        rsp_len) {
        ALOGE("rsp_result_and_data_length %u is inconsistent with rsp_len %u",
              rsp_buf->rsp_result_and_data_length, rsp_len);
        return -1;
    }

    *keyBlobLength = key_blob->key_blob_length;
    /* freeing memory allocated here is the responsibility of the caller several layers above */
    *keyBlob = (uint8_t *)malloc(key_blob->key_blob_length);
    if (*keyBlob == NULL) {
        ALOGE("memory allocation for keyBlob failed");
        return -1;
    }

    memcpy(*keyBlob, key_blob->key_blob, key_blob->key_blob_length);

    return 0;
}

EVP_PKEY* intel_keymaster_unwrap_key(const uint8_t* keyBlob, const size_t keyBlobLength)
{
    UniquePtr<intel_keymaster_firmware_cmd_t> cmd_buf(
       static_cast<intel_keymaster_firmware_cmd_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    UniquePtr<intel_keymaster_firmware_rsp_t> rsp_buf(
       static_cast<intel_keymaster_firmware_rsp_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    intel_keymaster_key_blob_t *key_blob;
    uint32_t cmd_len, rsp_len;
    sep_keymaster_return_t sep_ret;
    intel_keymaster_public_key_t *public_key;
    intel_keymaster_rsa_public_key_t *rsa_public_key;
    uint32_t rsp_result_and_data_length;

    if ((keyBlob == NULL) || (keyBlobLength == 0)) {
        return NULL;
    }

    if (cmd_buf.get() == NULL) {
        return NULL;
    }

    if (rsp_buf.get() == NULL) {
        return NULL;
    }

    cmd_buf->cmd_id = KEYMASTER_CMD_GET_KEYPAIR_PUBLIC;
    cmd_buf->cmd_data_length = keyBlobLength + offsetof(intel_keymaster_key_blob_t, key_blob);
    cmd_len = cmd_buf->cmd_data_length + offsetof(intel_keymaster_firmware_cmd_t, cmd_data);

    if (cmd_len > INTEL_KEYMASTER_MAX_MESSAGE_SIZE) {
        ALOGE("keyBlob is too big, keyBlobLength = %u", keyBlobLength);
        return NULL;
    }

    key_blob = (intel_keymaster_key_blob_t *)cmd_buf->cmd_data;
    key_blob->key_blob_length = keyBlobLength;
    memcpy(key_blob->key_blob, keyBlob, keyBlobLength);

    rsp_len = INTEL_KEYMASTER_MAX_MESSAGE_SIZE;
    sep_ret = sep_keymaster_send_cmd((uint8_t *)cmd_buf.get(), cmd_len, (uint8_t *)rsp_buf.get(),
                                     &rsp_len);

    if (sep_ret != SEP_KEYMASTER_SUCCESS) {
        ALOGE("sep_keymaster_send_cmd() returned error %d", sep_ret);
        return NULL;
    }

    // response should contain at least rsp_id, rsp_result_and_data_length and result
    if (rsp_len < offsetof(intel_keymaster_firmware_rsp_t, rsp_data)) {
        ALOGE("response is too short, rsp_len = %u", rsp_len);
        return NULL;
    }

    // rsp_result_and_data_length should include at least the result field
    if (rsp_buf->rsp_result_and_data_length < sizeof(intel_keymaster_result_t)) {
        ALOGE("rsp_result_and_data_length too small, rsp_result_and_data_length = %u",
              rsp_buf->rsp_result_and_data_length);
        return NULL;
    }

    // the result must indicate success
    if (rsp_buf->result != KEYMASTER_RESULT_SUCCESS) {
        ALOGE("firmware returned result is not successful, result = 0x%x", rsp_buf->result);
        return NULL;
    }

    public_key = (intel_keymaster_public_key_t *)rsp_buf->rsp_data;

    if (public_key->key_type != KEY_TYPE_RSA) {
        ALOGE("key type other than RSA is not yet supported");
        return NULL;
    }

    rsa_public_key = (intel_keymaster_rsa_public_key_t *)public_key->public_key_data;

    rsp_result_and_data_length = rsa_public_key->modulus_length +
        rsa_public_key->public_exponent_length +
        offsetof(intel_keymaster_rsa_public_key_t, bytes) +
        offsetof(intel_keymaster_public_key_t, public_key_data) +
        sizeof(intel_keymaster_result_t);
    if (rsp_result_and_data_length != rsp_buf->rsp_result_and_data_length) {
        ALOGE("calculated rsp_result_and_data_length %u does not match rsp_result_and_data_length "
              "%u in structure",
              rsp_result_and_data_length, rsp_buf->rsp_result_and_data_length);
        return NULL;
    }

    if (rsp_buf->rsp_result_and_data_length + offsetof(intel_keymaster_firmware_rsp_t, result) !=
        rsp_len) {
        ALOGE("rsp_result_and_data_length %u is inconsistent with rsp_len %u",
              rsp_buf->rsp_result_and_data_length, rsp_len);
        return NULL;
    }

    // convert to DER encoded format
    Unique_RSA rsa(RSA_new());
    if (rsa.get() == NULL) {
        ALOGE("Memory allocation for rsa failed");
        return NULL;
    }

    rsa->n = BN_bin2bn(rsa_public_key->bytes, rsa_public_key->modulus_length, NULL);
    if (rsa->n == NULL) {
        ALOGE("Couldn't create modulus from public key byte array");
        return NULL;
    }

    rsa->e = BN_bin2bn(rsa_public_key->bytes + rsa_public_key->modulus_length,
                       rsa_public_key->public_exponent_length, NULL);
    if (rsa->e == NULL) {
        ALOGE("Couldn't create public exponent from public key byte array");
        return NULL;
    }

    Unique_EVP_PKEY pkey(EVP_PKEY_new());
    if (pkey.get() == NULL) {
        ALOGE("Memory allocation for pkey failed");
        return NULL;
    }

    if (!EVP_PKEY_assign_RSA(pkey.get(), rsa.get())) {
        ALOGE("Couldn't assign rsa to pkey\n");
        return NULL;
    }

    OWNERSHIP_TRANSFERRED(rsa);

    return pkey.release();
}

int intel_keymaster_sign_rsa(const uint8_t* keyBlob, const size_t keyBlobLength,
                             const uint8_t* data, const size_t dataLength,
                             uint8_t** signedData, size_t* signedDataLength)
{
    UniquePtr<intel_keymaster_firmware_cmd_t> cmd_buf(
       static_cast<intel_keymaster_firmware_cmd_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    UniquePtr<intel_keymaster_firmware_rsp_t> rsp_buf(
       static_cast<intel_keymaster_firmware_rsp_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    intel_keymaster_signing_cmd_data_t *signing_data;
    intel_keymaster_signature_t *signature;
    uint32_t cmd_len, rsp_len;
    sep_keymaster_return_t sep_ret;
    uint32_t rsp_result_and_data_length;

    if ((keyBlob == NULL) || (data == NULL) || (signedData == NULL) || (signedDataLength == NULL))
    {
        return -1;
    }

    if ((keyBlobLength == 0) || (dataLength == 0)) {
        return -1;
    }

    if (cmd_buf.get() == NULL) {
        return -1;
    }

    if (rsp_buf.get() == NULL) {
        return -1;
    }

    cmd_buf->cmd_id = KEYMASTER_CMD_SIGN_DATA;
    cmd_buf->cmd_data_length = keyBlobLength + dataLength +
        offsetof(intel_keymaster_signing_cmd_data_t, buffer);
    cmd_len = cmd_buf->cmd_data_length + offsetof(intel_keymaster_firmware_cmd_t, cmd_data);

    if (cmd_len > INTEL_KEYMASTER_MAX_MESSAGE_SIZE) {
        ALOGE("combination of keyblob and data length is too big, keyBlobLength = %u, "
              "dataLength = %u",
              keyBlobLength, dataLength);
        return -1;
    }

    signing_data = (intel_keymaster_signing_cmd_data_t *)cmd_buf->cmd_data;
    signing_data->key_blob_length = keyBlobLength;
    signing_data->data_length = dataLength;
    memcpy(signing_data->buffer, keyBlob, keyBlobLength);
    memcpy(signing_data->buffer + keyBlobLength, data, dataLength);

    rsp_len = INTEL_KEYMASTER_MAX_MESSAGE_SIZE;
    sep_ret = sep_keymaster_send_cmd((uint8_t *)cmd_buf.get(), cmd_len, (uint8_t *)rsp_buf.get(),
                                     &rsp_len);

    if (sep_ret != SEP_KEYMASTER_SUCCESS) {
        ALOGE("sep_keymaster_send_cmd() returned error %d", sep_ret);
        return -1;
    }

    // response should contain at least rsp_id, rsp_result_and_data_length and result
    if (rsp_len < offsetof(intel_keymaster_firmware_rsp_t, rsp_data)) {
        ALOGE("response is too short, rsp_len = %u", rsp_len);
        return -1;
    }

    // rsp_result_and_data_length should include at least the result field
    if (rsp_buf->rsp_result_and_data_length < sizeof(intel_keymaster_result_t)) {
        ALOGE("rsp_result_and_data_length too small, rsp_result_and_data_length = %u",
              rsp_buf->rsp_result_and_data_length);
        return -1;
    }

    // the result must indicate success
    if (rsp_buf->result != KEYMASTER_RESULT_SUCCESS) {
        ALOGE("firmware returned result is not successful, result = 0x%x", rsp_buf->result);
        return -1;
    }

    signature = (intel_keymaster_signature_t *)rsp_buf->rsp_data;

    if (signature->signature_length == 0) {
        ALOGE("signature_length is 0");
        return -1;
    }

    rsp_result_and_data_length = signature->signature_length +
        offsetof(intel_keymaster_signature_t, signature) + sizeof(intel_keymaster_result_t);
    if (rsp_result_and_data_length != rsp_buf->rsp_result_and_data_length) {
        ALOGE("calculated rsp_result_and_data_length %u does not match rsp_result_and_data_length "
              "%u in structure",
              rsp_result_and_data_length, rsp_buf->rsp_result_and_data_length);
        return -1;
    }

    if (rsp_buf->rsp_result_and_data_length + offsetof(intel_keymaster_firmware_rsp_t, result) !=
        rsp_len) {
        ALOGE("rsp_result_and_data_length %u is inconsistent with rsp_len %u",
              rsp_buf->rsp_result_and_data_length, rsp_len);
        return -1;
    }

    *signedDataLength = signature->signature_length;
    /* freeing memory allocated here is the responsibility of the caller several layers above */
    *signedData = (uint8_t *)malloc(signature->signature_length);
    if (*signedData == NULL) {
        ALOGE("memory allocation for signedData failed");
        return -1;
    }

    memcpy(*signedData, signature->signature, signature->signature_length);

    return 0;
}

int intel_keymaster_verify_rsa(const uint8_t* keyBlob, const size_t keyBlobLength,
                               const uint8_t* signedData, const size_t signedDataLength,
                               const uint8_t* signature, const size_t signatureLength)
{
    UniquePtr<intel_keymaster_firmware_cmd_t> cmd_buf(
       static_cast<intel_keymaster_firmware_cmd_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    UniquePtr<intel_keymaster_firmware_rsp_t> rsp_buf(
       static_cast<intel_keymaster_firmware_rsp_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    intel_keymaster_verification_data_t *verification_data;
    uint32_t cmd_len, rsp_len;
    sep_keymaster_return_t sep_ret;

    if ((keyBlob == NULL) || (signedData == NULL) || (signature == NULL)) {
        return -1;
    }

    if ((keyBlobLength == 0) || (signedDataLength == 0) || (signatureLength == 0)) {
        return -1;
    }

    if (cmd_buf.get() == NULL) {
        return -1;
    }

    if (rsp_buf.get() == NULL) {
        return -1;
    }

    cmd_buf->cmd_id = KEYMASTER_CMD_VERIFY_DATA;
    cmd_buf->cmd_data_length = keyBlobLength + signedDataLength + signatureLength +
        offsetof(intel_keymaster_verification_data_t, buffer);
    cmd_len = cmd_buf->cmd_data_length + offsetof(intel_keymaster_firmware_cmd_t, cmd_data);

    if (cmd_len > INTEL_KEYMASTER_MAX_MESSAGE_SIZE) {
        ALOGE("combination of keyBlobLength, signedDataLength and signatureLength is too big, "
              "keyBlobLength = %u, signedDataLength = %u, signatureLength = %u",
              keyBlobLength, signedDataLength, signatureLength);
        return -1;
    }

    verification_data = (intel_keymaster_verification_data_t *)cmd_buf->cmd_data;
    verification_data->key_blob_length = keyBlobLength;
    verification_data->data_length = signedDataLength;
    verification_data->signature_length = signatureLength;
    memcpy(verification_data->buffer, keyBlob, keyBlobLength);
    memcpy(verification_data->buffer + keyBlobLength, signedData, signedDataLength);
    memcpy(verification_data->buffer + keyBlobLength + signedDataLength, signature,
           signatureLength);

    rsp_len = INTEL_KEYMASTER_MAX_MESSAGE_SIZE;
    sep_ret = sep_keymaster_send_cmd((uint8_t *)cmd_buf.get(), cmd_len, (uint8_t *)rsp_buf.get(),
                                     &rsp_len);

    if (sep_ret != SEP_KEYMASTER_SUCCESS) {
        ALOGE("sep_keymaster_send_cmd() returned error %d", sep_ret);
        return -1;
    }

    // response should contain at least rsp_id, rsp_result_and_data_length and result
    if (rsp_len < offsetof(intel_keymaster_firmware_rsp_t, rsp_data)) {
        ALOGE("response is too short, rsp_len = %u", rsp_len);
        return -1;
    }

    // rsp_result_and_data_length should include the result field only
    if (rsp_buf->rsp_result_and_data_length != sizeof(intel_keymaster_result_t)) {
        ALOGE("rsp_result_and_data_length unexpected, rsp_result_and_data_length = %u",
              rsp_buf->rsp_result_and_data_length);
        return -1;
    }

    // the result must indicate success
    if (rsp_buf->result != KEYMASTER_RESULT_SUCCESS) {
        ALOGE("firmware returned result is not successful, result = 0x%x", rsp_buf->result);
        return -1;
    }

    return 0;
}

int intel_keymaster_delete_keypair(const struct keymaster_device*,
        const uint8_t* keyBlob, const size_t keyBlobLength)
{
    UniquePtr<intel_keymaster_firmware_cmd_t> cmd_buf(
       static_cast<intel_keymaster_firmware_cmd_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    UniquePtr<intel_keymaster_firmware_rsp_t> rsp_buf(
       static_cast<intel_keymaster_firmware_rsp_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    intel_keymaster_key_blob_t *key_blob;
    uint32_t cmd_len, rsp_len;
    sep_keymaster_return_t sep_ret;

    if ((keyBlob == NULL) || (keyBlobLength == 0)) {
        return -1;
    }

    if (cmd_buf.get() == NULL) {
        return -1;
    }

    if (rsp_buf.get() == NULL) {
        return -1;
    }

    cmd_buf->cmd_id = KEYMASTER_CMD_DELETE_KEYPAIR;
    cmd_buf->cmd_data_length = keyBlobLength + offsetof(intel_keymaster_key_blob_t, key_blob);
    cmd_len = cmd_buf->cmd_data_length + offsetof(intel_keymaster_firmware_cmd_t, cmd_data);

    if (cmd_len > INTEL_KEYMASTER_MAX_MESSAGE_SIZE) {
        ALOGE("keyBlob is too big, keyBlobLength = %u", keyBlobLength);
        return -1;
    }

    key_blob = (intel_keymaster_key_blob_t *)cmd_buf->cmd_data;
    key_blob->key_blob_length = keyBlobLength;
    memcpy(key_blob->key_blob, keyBlob, keyBlobLength);

    rsp_len = INTEL_KEYMASTER_MAX_MESSAGE_SIZE;
    sep_ret = sep_keymaster_send_cmd((uint8_t *)cmd_buf.get(), cmd_len, (uint8_t *)rsp_buf.get(),
                                     &rsp_len);

    if (sep_ret != SEP_KEYMASTER_SUCCESS) {
        ALOGE("sep_keymaster_send_cmd() returned error %d", sep_ret);
        return -1;
    }

    // response should contain at least rsp_id, rsp_result_and_data_length and result
    if (rsp_len < offsetof(intel_keymaster_firmware_rsp_t, rsp_data)) {
        ALOGE("response is too short, rsp_len = %u", rsp_len);
        return -1;
    }

    // rsp_result_and_data_length should include at least the result field
    if (rsp_buf->rsp_result_and_data_length < sizeof(intel_keymaster_result_t)) {
        ALOGE("rsp_result_and_data_length too small, rsp_result_and_data_length = %u",
              rsp_buf->rsp_result_and_data_length);
        return -1;
    }

    // the result must indicate success
    if (rsp_buf->result != KEYMASTER_RESULT_SUCCESS) {
        ALOGE("firmware returned result is not successful, result = 0x%x", rsp_buf->result);
        return -1;
    }

    return 0;
}

int intel_keymaster_delete_all(const struct keymaster_device*)
{
    UniquePtr<intel_keymaster_firmware_cmd_t> cmd_buf(
       static_cast<intel_keymaster_firmware_cmd_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    UniquePtr<intel_keymaster_firmware_rsp_t> rsp_buf(
       static_cast<intel_keymaster_firmware_rsp_t *>(malloc(INTEL_KEYMASTER_MAX_MESSAGE_SIZE)));
    uint32_t cmd_len, rsp_len;
    sep_keymaster_return_t sep_ret;

    if (cmd_buf.get() == NULL) {
        return -1;
    }

    if (rsp_buf.get() == NULL) {
        return -1;
    }

    cmd_buf->cmd_id = KEYMASTER_CMD_DELETE_ALL;
    cmd_buf->cmd_data_length = 0;
    cmd_len = cmd_buf->cmd_data_length + offsetof(intel_keymaster_firmware_cmd_t, cmd_data);

    rsp_len = INTEL_KEYMASTER_MAX_MESSAGE_SIZE;
    sep_ret = sep_keymaster_send_cmd((uint8_t *)cmd_buf.get(), cmd_len, (uint8_t *)rsp_buf.get(),
                                     &rsp_len);

    if (sep_ret != SEP_KEYMASTER_SUCCESS) {
        ALOGE("sep_keymaster_send_cmd() returned error %d", sep_ret);
        return -1;
    }

    // response should contain at least rsp_id, rsp_result_and_data_length and result
    if (rsp_len < offsetof(intel_keymaster_firmware_rsp_t, rsp_data)) {
        ALOGE("response is too short, rsp_len = %u", rsp_len);
        return -1;
    }

    // rsp_result_and_data_length should include at least the result field
    if (rsp_buf->rsp_result_and_data_length < sizeof(intel_keymaster_result_t)) {
        ALOGE("rsp_result_and_data_length too small, rsp_result_and_data_length = %u",
              rsp_buf->rsp_result_and_data_length);
        return -1;
    }

    // the result must indicate success
    if (rsp_buf->result != KEYMASTER_RESULT_SUCCESS) {
        ALOGE("firmware returned result is not successful, result = 0x%x", rsp_buf->result);
        return -1;
    }

    return 0;
}

/* keymaster module */

typedef UniquePtr<keymaster_device_t> Unique_keymaster_device_t;

/* Close an opened intel keymaster instance */
static int intel_keymaster_close(hw_device_t *dev) {
    delete dev;
    return 0;
}

/*
 * Generic device handling
 */
static int intel_keymaster_open(const hw_module_t* module, const char* name,
        hw_device_t** device) {
    if (strcmp(name, KEYSTORE_KEYMASTER) != 0)
        return -EINVAL;

    Unique_keymaster_device_t dev(new keymaster_device_t);
    if (dev.get() == NULL)
        return -ENOMEM;

    char platform[PATH_MAX] = {0};
    property_get("ro.board.platform", platform, NULL);

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 1;
    dev->common.module = (struct hw_module_t*) module;
    dev->common.close = intel_keymaster_close;

    dev->flags = 0;

    dev->generate_keypair = intel_keymaster_generate_keypair;
    dev->import_keypair = intel_keymaster_import_keypair;
    dev->get_keypair_public = intel_keymaster_get_keypair_public;
    if (strcmp(platform, "merrifield") == 0 ||
        strcmp(platform, "moorefield") == 0) {
        dev->delete_keypair = intel_keymaster_delete_keypair;
        dev->delete_all = intel_keymaster_delete_all;
    }
    else {
        dev->delete_keypair = NULL;
        dev->delete_all = NULL;
    }
    dev->sign_data = intel_keymaster_sign_data;
    dev->verify_data = intel_keymaster_verify_data;

    ERR_load_crypto_strings();
    ERR_load_BIO_strings();

    *device = reinterpret_cast<hw_device_t*>(dev.release());

    return 0;
}

static struct hw_module_methods_t keystore_module_methods = {
    open: intel_keymaster_open,
};

struct keystore_module HAL_MODULE_INFO_SYM
__attribute__ ((visibility ("default"))) = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        module_api_version: HARDWARE_MODULE_API_VERSION(1,0),
        hal_api_version: HARDWARE_HAL_API_VERSION,
        id: KEYSTORE_HARDWARE_MODULE_ID,
        name: "Keymaster Intel HAL",
        author: "The Android Open Source Project",
        methods: &keystore_module_methods,
        dso: 0,
        reserved: {},
    },
};
