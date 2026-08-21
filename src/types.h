#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "bip32.h"

#include "constants.h"
#include "tx_types.h"
#include "canonical_hash.h"
#include "nbgl_use_case.h"  // for nbgl_contentTagValue_t

/**
 * Enumeration with expected INS of APDU commands.
 */
typedef enum {
    GET_VERSION = 0x03,     /// version of the application
    GET_APP_NAME = 0x04,    /// name of the application
    GET_PUBLIC_KEY = 0x05,  /// public key of corresponding BIP32 path
    SIGN_TX = 0x06          /// sign transaction with BIP32 path
} command_e;
/**
 * Enumeration with parsing state.
 */
typedef enum {
    STATE_NONE,            /// No state
    STATE_EXPECTING_MORE,  /// Expecting more data (for chunked APDU)
    STATE_MSG_COMPLETE,    /// Message completely received, more expected
    STATE_PARSED,          /// All transaction data parsed
    STATE_APPROVED         /// Transaction data approved
} state_e;

/**
 * Enumeration with user request type.
 */
typedef enum {
    CONFIRM_ADDRESS = 0,      /// confirm address derived from public key
    CONFIRM_TRANSACTION = 1,  /// confirm transaction before signing
} request_type_e;

typedef enum {
    SIGN_HASH = 0,
    SIGN_UNTYPED_VERSIONED_MESSAGE = 1,
    SIGN_PREPARED_TRANSACTION = 2,
} signing_type_e;
/**
 * Structure for public key context information.
 */
typedef struct {
    uint8_t raw_public_key[ED25519_RAW_PUBLIC_KEY_LEN];  /// ED25519 public key raw format
    uint8_t chain_code[MAX_CHAINCODE_LEN];               /// for public key derivation
} pubkey_ctx_t;

/**
 * Structure for transaction information context.
 */
typedef struct {
    uint8_t raw_tx[MAX_TRANSACTION_LEN];  /// raw transaction serialized
    size_t raw_tx_len;                    /// length of raw transaction

    transaction_parts_ctx_t tx_parts_ctx;      /// transaction parts context
    HashWriter hasher;                         // Dynamically allocated hasher
    uint8_t partial_tx_hash[SHA256_HASH_LEN];  // Incomplete tx hash
    uint8_t partial_md_hash[SHA256_HASH_LEN];  // Incomplete md hash
    int32_t
        recv_node_idx;  // Index of the current entity being processed ('Node' or 'InputContract')

    uint8_t m_hash[CANTON_HASH_LEN];  /// message hash digest (can either be SHA-256 or Canton hash)
    uint8_t m_hash_len;               /// length of message hash digest
    uint8_t signature[ED25519_SIG_LEN];            /// transaction signature (ED25519)
    uint8_t signature_len;                         /// length of transaction signature
    uint8_t challenge_signature[ED25519_SIG_LEN];  /// challenge signature (ED25519)
    uint8_t challenge_signature_len;               /// length of challenge signature
    bool has_challenge_signature;                  /// whether challenge signature is present

    uint8_t v;                      /// parity of y-coordinate of R in ECDSA signature
    nbgl_contentTagValue_t *pairs;  // dynamically allocated array for display
    size_t pairs_count;
    bool clear_signing_available;  /// whether clearing signing data is allowed
    const char *review_title;      /// dynamically allocated review title
    const char *review_finish;

} transaction_ctx_t;

/**
 * Structure for global context.
 */
typedef struct {
    state_e state;  /// state of the context
    union {
        pubkey_ctx_t pk_info;       /// public key context
        transaction_ctx_t tx_info;  /// transaction context
    };
    request_type_e req_type;              /// user request
    signing_type_e signing_type;          /// signing type (for CONFIRM_TRANSACTION)
    uint32_t bip32_path[MAX_BIP32_PATH];  /// BIP32 path
    uint8_t bip32_path_len;               /// length of BIP32 path
} global_ctx_t;
