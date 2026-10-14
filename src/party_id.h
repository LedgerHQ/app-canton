#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include "utils.h"
#include "constants.h"

/**
 * Convert public key to party id. Party id is defined as the concatenation of:
 * - the prefix "ldg::"
 * - the lowercase hex encoding of the 34-byte key fingerprint, which is:
 *  - the multihash prefix 0x12 0x20 (SHA-256, 32 bytes)
 *  - the sha256 hash of the hash purpose 12 (as 4 bytes, big-endian) and the raw public key
 *
 * party id = "ldg::" hex(0x12 || 0x20 || sha256(u32_be(12) || public_key))
 *
 * @param[in]  public_key
 *   Pointer to byte buffer with the raw Ed25519 public key (32 bytes).
 * @param[out] out
 *   Pointer to output byte buffer for the null-terminated party id.
 * @param[in]  out_len
 *   Length of output byte buffer.
 *
 * @return true if success, false otherwise.
 *
 */
MUST_CHECK bool party_id_from_pubkey(const uint8_t public_key[ED25519_RAW_PUBLIC_KEY_LEN],
                                     uint8_t *out,
                                     size_t out_len);
