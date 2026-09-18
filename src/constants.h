#pragma once

/**
 * Instruction class of the Boilerplate application.
 */
#define CLA 0xE0

/**
 * Length of APPNAME variable in the Makefile.
 */
#define APPNAME_LEN (sizeof(APPNAME) - 1)

/**
 * Maximum length of MAJOR_VERSION || MINOR_VERSION || PATCH_VERSION.
 */
#define APPVERSION_LEN 3

/**
 * Maximum length of application name.
 */
#define MAX_APPNAME_LEN 64

/**
 * Maximum transaction length (bytes).
 */
#define MAX_TRANSACTION_LEN (1024 * 12)

/**
 * Maximum number of child nodes per transaction node.
 * This also sizes the node hash store in canonical_hash.c, because a parent needs its children's
 * hashes and nothing else. It is not a limit on how many nodes a transaction may have.
 */
#define MAX_NODE_CHILDREN 32

/**
 * Maximum number of nodes in a transaction that the node tree check tracks.
 * The tracking array is allocated for the announced node count, so this only bounds that
 * allocation. A transaction with more nodes is blind signed instead of clear signed.
 */
#define MAX_TX_NODES 256

/**
 * Maximum number of distinct contracts a transaction's nodes may act on, and the length of the
 * digest kept per contract. 64 bits is enough here: a forged match would only let a disclosed
 * contract that no node used pass, which is inert. 32 entries covers the widest recorded
 * transaction, which touches 28.
 */
#define MAX_USED_CONTRACTS 32
#define USED_CONTRACT_LEN  8

/**
 * Maximum number of value-holding create nodes tracked in a transaction.
 * Real transactions write three: one for the receiver, one for the change, one for a fee.
 */
#define MAX_HOLDINGS 4

/**
 * ED25519 signature length (bytes).
 */
#define ED25519_SIG_LEN 64

/**
 * Exponent used to convert mBOL to BOL unit (N BOL = N * 10^3 mBOL).
 */
#define EXPONENT_SMALLEST_UNIT 3

/**
 * ED25519 public key raw format length
 */
#define ED25519_RAW_PUBLIC_KEY_LEN 32

/**
 * Maximum code chain length (bytes).
 */
#define MAX_CHAINCODE_LEN 32

/**
 * Length of SHA-256 hash.
 */
#define SHA256_HASH_LEN 32

/**
 * Length of Canton hash (2 bytes prefix + SHA-256 hash).
 */
#define CANTON_HASH_LEN (SHA256_HASH_LEN + 2)

/**
 * Length of uint32_t when serialized in big-endian format.
 */
#define UINT32_T_LEN 4

/**
 * Length of uint64_t when serialized in big-endian format.
 */
#define UINT64_T_LEN 8

/**
 * Default buffer size for decoding strings.
 */
#define DEFAULT_DECODE_BUFFER_SIZE 64
