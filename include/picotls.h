/*
 * Copyright (c) 2016 DeNA Co., Ltd., Kazuho Oku
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifndef picotls_h
#define picotls_h

#include <assert.h>
#include <inttypes.h>

#define PTLS_MAX_SECRET_SIZE 32
#define PTLS_MAX_IV_SIZE 16
#define PTLS_MAX_DIGEST_SIZE 64

/* cipher-suites */
#define PTLS_CIPHER_SUITE_AES_128_GCM_SHA256 0x1301
#define PTLS_CIPHER_SUITE_AES_256_GCM_SHA384 0x1302
#define PTLS_CIPHER_SUITE_CHACHA20_POLY1305_SHA256 0x1303

/* negotiated_groups */
#define PTLS_GROUP_SECP256R1 23
#define PTLS_GROUP_X25519 29

/* signature algorithms */
#define PTLS_SIGNATURE_RSA_PKCS1_SHA1 0x0201
#define PTLS_SIGNATURE_RSA_PKCS1_SHA256 0x0401
#define PTLS_SIGNATURE_ECDSA_SECP256R1_SHA256 0x0403
#define PTLS_SIGNATURE_RSA_PSS_SHA256 0x0804

/* error classes and macros */
#define PTLS_ERROR_CLASS_SELF_ALERT 0
#define PTLS_ERROR_CLASS_PEER_ALERT 0x100
#define PTLS_ERROR_CLASS_INTERNAL 0x200

#define PTLS_ERROR_GET_CLASS(e) ((e) & ~0xff)
#define PTLS_ALERT_TO_SELF_ERROR(e) ((e) + PTLS_ERROR_CLASS_SELF_ALERT)
#define PTLS_ALERT_TO_PEER_ERROR(e) ((e) + PTLS_ERROR_CLASS_PEER_ALERT)
#define PTLS_ERROR_TO_ALERT(e) ((e)&0xff)

/* alerts */
#define PTLS_ALERT_CLOSE_NOTIFY 0
#define PTLS_ALERT_END_OF_EARLY_DATA 1
#define PTLS_ALERT_UNEXPECTED_MESSAGE 10
#define PTLS_ALERT_BAD_RECORD_MAC 20
#define PTLS_ALERT_HANDSHAKE_FAILURE 40
#define PTLS_ALERT_BAD_CERTIFICATE 42
#define PTLS_ALERT_CERTIFICATE_REVOKED 44
#define PTLS_ALERT_CERTIFICATE_EXPIRED 45
#define PTLS_ALERT_CERTIFICATE_UNKNOWN 46
#define PTLS_ALERT_ILLEGAL_PARAMETER 47
#define PTLS_ALERT_DECODE_ERROR 50
#define PTLS_ALERT_DECRYPT_ERROR 51
#define PTLS_ALERT_INTERNAL_ERROR 80
#define PTLS_ALERT_USER_CANCELED 90
#define PTLS_ALERT_MISSING_EXTENSION 109
#define PTLS_ALERT_UNRECOGNIZED_NAME 112

/* internal errors */
#define PTLS_ERROR_NO_MEMORY (PTLS_ERROR_CLASS_INTERNAL + 1)
#define PTLS_ERROR_HANDSHAKE_IN_PROGRESS (PTLS_ERROR_CLASS_INTERNAL + 2)
#define PTLS_ERROR_LIBRARY (PTLS_ERROR_CLASS_INTERNAL + 3)
#define PTLS_ERROR_INCOMPATIBLE_KEY (PTLS_ERROR_CLASS_INTERNAL + 4)

typedef struct st_ptls_t ptls_t;

/**
 * represents a sequence of octets
 */
typedef struct st_ptls_iovec_t {
    uint8_t *base;
    size_t len;
} ptls_iovec_t;

/**
 * used for storing output
 */
typedef struct st_ptls_buffer_t {
    uint8_t *base;
    size_t capacity;
    size_t off;
    int is_allocated;
} ptls_buffer_t;

typedef const struct st_ptls_crypto_t ptls_crypto_t;

/**
 * defines callbacks for certificate-related operations during the handshake
 */
typedef struct st_ptls_certificate_context_t {
    /**
     * after receiving ClientHello, the core calls the callback to obtain the certificate chain to be sent to the client as well as
     * a pointer to a function that should be called for signing the handshake using the private key associated to the certificate
     */
    int (*lookup)(ptls_t *tls, uint16_t *sign_algorithm, int (**signer)(void *sign_ctx, ptls_iovec_t *output, ptls_iovec_t input),
                  void **signer_data, ptls_iovec_t **certs, size_t *num_certs, ptls_iovec_t server_name,
                  const uint16_t *signature_algorithms, size_t num_signature_algorithms);
    /**
     * after receiving Certificate, the core calls the callback to verify the certificate chain and to obtain a pointer to a
     * callback that should be used for verifying CertificateVerify. If an error occurs between a successful return from this
     * callback to the invocation of the verify_sign callback, verify_sign is called with both data and sign set to an empty buffer.
     * The implementor of the callback should use that as the opportunity to free any temporary data allocated for the verify_sign
     * callback.
     */
    int (*verify)(ptls_t *tls, int (**verify_sign)(void *verify_ctx, ptls_iovec_t data, ptls_iovec_t sign), void **verify_data,
                  ptls_iovec_t *certs, size_t num_certs);
} ptls_certificate_context_t;

/**
 * key exchange context built by ptls_key_exchange_algorithm::create.
 */
typedef struct st_ptls_key_exchange_context_t {
    /**
     * called once per created context. It is the callee's responsibility to free the resources associated to keyex. Secret and
     * peerkey will be NULL in case the exchange never happened.
     */
    int (*on_exchange)(struct st_ptls_key_exchange_context_t *keyex, ptls_iovec_t *secret, ptls_iovec_t peerkey);
} ptls_key_exchange_context_t;

/**
 * A key exchange algorithm.
 */
typedef const struct st_ptls_key_exchange_algorithm_t {
    /**
     * ID defined by the TLS specification
     */
    uint16_t id;
    /**
     * creates a context for asynchronous key exchange. The function is called when ClientHello is generated. The on_exchange
     * callback of the created context is called when the client receives ServerHello.
     */
    int (*create)(ptls_key_exchange_context_t **ctx, ptls_iovec_t *pubkey);
    /**
     * implements synchronous key exchange. Called when receiving a ServerHello.
     */
    int (*exchange)(ptls_iovec_t *pubkey, ptls_iovec_t *secret, ptls_iovec_t peerkey);
} ptls_key_exchange_algorithm_t;

/**
 * AEAD context. AEAD implementations are allowed to stuff data at the end of the struct. The size of the memory allocated for the
 * struct is governed by ptls_aead_algorithm_t::context_size.
 */
typedef struct st_ptls_aead_context_t {
    const struct st_ptls_aead_algorithm_t *algo;
    uint64_t seq;
    uint8_t static_iv[PTLS_MAX_IV_SIZE];
    /* field above this line must not be altered by the crypto binding */
    void (*dispose_crypto)(struct st_ptls_aead_context_t *ctx);
    int (*do_transform)(struct st_ptls_aead_context_t *ctx, void *output, size_t *outlen, const void *input, size_t inlen,
                        const void *iv, uint8_t enc_content_type);
} ptls_aead_context_t;

/**
 * An AEAD cipher.
 */
typedef const struct st_ptls_aead_algorithm_t {
    /**
     * key size
     */
    size_t key_size;
    /**
     * size of the IV
     */
    size_t iv_size;
    /**
     * size of memory allocated for ptls_aead_context_t. AEAD implementations can set this value to something greater than
     * sizeof(ptls_aead_context_t) and stuff additional data at the bottom of the struct.
     */
    size_t context_size;
    /**
     * callback that sets up the crypto
     */
    int (*setup_crypto)(ptls_aead_context_t *ctx, int is_enc, const void *key);
} ptls_aead_algorithm_t;

/**
 *
 */
typedef enum en_ptls_hash_final_mode_t {
    /**
     * obtains the digest and frees the context
     */
    PTLS_HASH_FINAL_MODE_FREE = 0,
    /**
     * obtains the digest and reset the context to initial state
     */
    PTLS_HASH_FINAL_MODE_RESET = 1,
    /**
     * obtains the digest while leaving the context as-is
     */
    PTLS_HASH_FINAL_MODE_SNAPSHOT = 2
} ptls_hash_final_mode_t;

/**
 * A hash context.
 */
typedef struct st_ptls_hash_context_t {
    /**
     * feeds additional data into the hash context
     */
    void (*update)(struct st_ptls_hash_context_t *ctx, const void *src, size_t len);
    /**
     * returns the digest and performs necessary operation specified by mode
     */
    void (* final)(struct st_ptls_hash_context_t *ctx, void *md, ptls_hash_final_mode_t mode);
} ptls_hash_context_t;

/**
 * A hash algorithm and its properties.
 */
typedef const struct st_ptls_hash_algorithm_t {
    /**
     * block size
     */
    size_t block_size;
    /**
     * digest size
     */
    size_t digest_size;
    /**
     * constructor that creates the hash context
     */
    ptls_hash_context_t *(*create)(void);
} ptls_hash_algorithm_t;

typedef const struct st_ptls_cipher_suite_t {
    uint16_t id;
    ptls_aead_algorithm_t *aead;
    ptls_hash_algorithm_t *hash;
} ptls_cipher_suite_t;

/**
 * A list of ciphers and callbacks required for running TLS. Users can create this structure of their choice to enable / disable
 * certain ciphers.
 */
struct st_ptls_crypto_t {
    /**
     * PRNG to be used
     */
    void (*random_bytes)(void *buf, size_t len);
    /**
     * list of supported key-exchange algorithms terminated by NULL
     */
    ptls_key_exchange_algorithm_t **key_exchanges;
    /**
     * list of supported cipher-suites terminated by NULL
     */
    ptls_cipher_suite_t **cipher_suites;
};

/**
 * builds a new ptls_iovec_t instance using the supplied parameters
 */
static ptls_iovec_t ptls_iovec_init(const void *p, size_t len);
/**
 * initializes a buffer, setting the default destination to the small buffer provided as the argument.
 */
static void ptls_buffer_init(ptls_buffer_t *buf, void *smallbuf, size_t smallbuf_size);
/**
 * disposes a buffer, freeing resources allocated by the buffer itself (if any)
 */
static void ptls_buffer_dispose(struct st_ptls_buffer_t *buf);
/**
 * internal
 */
void ptls_buffer__release_memory(struct st_ptls_buffer_t *buf);
/**
 * reserves space for additional amount of memory
 */
int ptls_buffer_reserve(struct st_ptls_buffer_t *buf, size_t delta);

/**
 * create a object to handle new TLS connection. Client-side of a TLS connection is created if server_name is non-NULL. Otherwise,
 * a server-side connection is created.
 */
ptls_t *ptls_new(ptls_crypto_t *crypto, ptls_certificate_context_t *cert_ctx, const char *server_name);
/**
 * releases all resources associated to the object
 */
void ptls_free(ptls_t *tls);
/**
 * returns address of the crypto callbacks that the connection is using
 */
ptls_crypto_t *ptls_get_crypto(ptls_t *tls);
/**
 * returns the certificate context that the connection is using
 */
ptls_certificate_context_t *ptls_get_certificate_context(ptls_t *tls);
/**
 * proceeds with the handshake, optionally taking some input from peer. The function returns zero in case the handshake completed
 * successfully. PTLS_ERROR_HANDSHAKE_IN_PROGRESS is returned in case the handshake is incomplete. Otherwise, an error value is
 * returned. The contents of sendbuf should be sent to the client, regardless of whether if an error is returned. inlen is an
 * argument used for both input and output. As an input, the arguments takes the size of the data available as input. Upon return
 * the value is updated to the number of bytes consumed by the handshake. In case the returned value is
 * PTLS_ERROR_HANDSHAKE_IN_PROGRESS there is a guarantee that all the input are consumed (i.e. the value of inlen does not change).
 */
int ptls_handshake(ptls_t *tls, ptls_buffer_t *sendbuf, const void *input, size_t *inlen);
/**
 * decrypts the first record within given buffer
 */
int ptls_receive(ptls_t *tls, ptls_buffer_t *plaintextbuf, const void *input, size_t *len);
/**
 * encrypts given buffer into multiple TLS records
 */
int ptls_send(ptls_t *tls, ptls_buffer_t *sendbuf, const void *input, size_t inlen);
/**
 *
 */
ptls_hash_context_t *ptls_hmac_create(ptls_hash_algorithm_t *algo, const void *key, size_t key_size);
/**
 *
 */
int ptls_hkdf_extract(ptls_hash_algorithm_t *hash, void *output, ptls_iovec_t salt, ptls_iovec_t ikm);
/**
 *
 */
int ptls_hkdf_expand(ptls_hash_algorithm_t *hash, void *output, size_t outlen, ptls_iovec_t prk, ptls_iovec_t info);
/**
 *
 */
ptls_aead_context_t *ptls_aead_new(ptls_aead_algorithm_t *aead, ptls_hash_algorithm_t *hash, int is_enc, const void *secret,
                                   const char *label);
/**
 *
 */
void ptls_aead_free(ptls_aead_context_t *ctx);
/**
 *
 */
int ptls_aead_transform(ptls_aead_context_t *ctx, void *output, size_t *outlen, const void *input, size_t inlen,
                        uint8_t enc_content_type);
/**
 * clears memory
 */
extern void (*volatile ptls_clear_memory)(void *p, size_t len);
/**
 *
 */
static ptls_iovec_t ptls_iovec_init(const void *p, size_t len);

/* inline functions */

inline ptls_iovec_t ptls_iovec_init(const void *p, size_t len)
{
    return (ptls_iovec_t){(uint8_t *)p, len};
}

inline void ptls_buffer_init(ptls_buffer_t *buf, void *smallbuf, size_t smallbuf_size)
{
    assert(smallbuf != NULL);
    buf->base = smallbuf;
    buf->off = 0;
    buf->capacity = smallbuf_size;
    buf->is_allocated = 0;
}

inline void ptls_buffer_dispose(ptls_buffer_t *buf)
{
    ptls_buffer__release_memory(buf);
    *buf = (ptls_buffer_t){NULL};
}

#endif