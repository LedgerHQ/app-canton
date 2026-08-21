#pragma once

#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include <stdint.h>   // uint*_t

#include "buffer.h"

#include "types.h"
#include "utils.h"

/**
 * Handler for GET_PUBLIC_KEY command. If successfully parse BIP32 path,
 * derive public key/chain code and send APDU response.
 *
 * @see G_context.bip32_path, G_context.pk_info.raw_public_key and
 *      G_context.pk_info.chain_code.
 *
 * @param[in,out] cdata
 *   Command data with BIP32 path.
 * @param[in]     display
 *   Whether to display address on screen or not.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
MUST_CHECK int handler_get_public_key(buffer_t *cdata, bool display);

/**
 * @brief Derives the public key and chain code from the given BIP32 path.
 *
 * @param bip32_path
 *   The BIP32 path to derive the key from.
 * @param bip32_path_len
 *   The length of the BIP32 path.
 * @param raw_public_key
 *   The buffer to store the derived public key.
 * @param chain_code
 *   The buffer to store the derived chain code.
 * @return cx_err_t
 *   CX_OK on success, an error code otherwise.
 */
MUST_CHECK cx_err_t derive_public_key(uint32_t *bip32_path,
                                      uint8_t bip32_path_len,
                                      uint8_t *raw_public_key,
                                      uint8_t *chain_code);
