#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include "utils.h"
#include "constants.h"

/**
 * Convert public key to party id. Party id is defined as:
 * the concatenation of:
 * - the hex-encoded ed25519 public key (32 bytes)
 * - a colon (':', 1 byte)
 * - the hex-encoded sha256 hash of the concatenation of:
 *  - a purpose byte (0x0C for party id)
 *  - the raw public key (32 bytes)
 *
 * party id = hex(public_key) : hex(sha256(hash purpose (12) || public_key))
 *
 * @param[in]  public_key
 *   Pointer to byte buffer with public key.
 *   The public key is represented as 32 bytes
 *   each coordinate.
 * @param[out] out
 *   Pointer to output byte buffer for address.
 * @param[in]  out_len
 *   Length of output byte buffer.
 *
 * @return true if success, false otherwise.
 *
 */
MUST_CHECK bool party_id_from_pubkey(const uint8_t public_key[ED25519_RAW_PUBLIC_KEY_LEN],
                                     uint8_t *out,
                                     size_t out_len);
