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
 */
#define MAX_NODE_CHILDREN 32

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
