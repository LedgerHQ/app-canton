/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <string.h>   // memset, explicit_bzero

#include "os.h"
#include "cx.h"
#include "buffer.h"
#include "status_words.h"

#include "sign_tx.h"
#include "sw.h"
#include "globals.h"
#include "display.h"
#include "tx_types.h"
#include "types.h"
#include "validate.h"
#include "canonical_hash.h"
#include "prepared_transaction.h"
#include "untyped_versioned_msg.h"
#include "mem.h"
#include "utils.h"

static int process_tx_chunk(buffer_t *cdata,
                            signing_type_e type,
                            bool first,
                            bool more,
                            bool msg_end);
static int process_transaction_hash(buffer_t *buffer);

buffer_t buf = {.ptr = NULL, .size = 0, .offset = 0};

MUST_CHECK int handler_sign_tx(buffer_t *cdata,
                               signing_type_e type,
                               bool first,
                               bool more,
                               bool msg_end) {
    int result = process_tx_chunk(cdata, type, first, more, msg_end);
    if (result != 0) {
        return io_send_sw(result);  // Send the error code via io_send_sw
    }

    if (G_context.signing_type != type) {
        PRINTF("Signing type mismatch: expected %d, got %d\n", G_context.signing_type, type);
        return io_send_sw(SW_BAD_STATE);
    }

    if (G_context.state == STATE_EXPECTING_MORE) {
        // More APDUs with transaction parts are expected.
        // Send a SW_OK to signal that we have received the chunk
        return io_send_sw(SW_OK);
    } else if (G_context.state == STATE_MSG_COMPLETE || G_context.state == STATE_PARSED) {
        // Process the complete message
        switch (G_context.signing_type) {
            case SIGN_PREPARED_TRANSACTION:
                result = process_prepared_tx_part(&buf);
                break;
            case SIGN_HASH:
                result = process_transaction_hash(&buf);
                break;
            case SIGN_UNTYPED_VERSIONED_MESSAGE:
                result = process_untyped_versioned_msg_tx(&buf);
                break;
            default:
                PRINTF("Unsupported signing type: %d\n", G_context.signing_type);
                return io_send_sw(SW_BAD_STATE);
        }
        if (result != 0) {
            return io_send_sw(result);  // Send the error code via io_send_sw
        }

        if (G_context.state == STATE_PARSED) {
            if (G_context.tx_info.clear_signing_available == true) {
                return ui_display_transaction();
            } else if (N_storage.allow_blind_sign == BlindSignDisabled) {
                return ui_error_blind_signing();
            } else {
                return ui_display_blind_signed_transaction();
            }
        } else {
            return io_send_sw(SW_OK);
        }
    } else {
        // Invalid state
        PRINTF("Invalid state after processing chunk: %d\n", G_context.state);
        return io_send_sw(SW_BAD_STATE);
    }
}

static int process_tx_chunk(buffer_t *cdata,
                            signing_type_e type,
                            bool first,
                            bool more,
                            bool msg_end) {
    LEDGER_ASSERT(cdata != NULL, "cdata is NULL");
    if (first) {  // first APDU, parse BIP32 path
        clean_context();
        PRINTF("Processing first chunk of transaction\n");
        G_context.req_type = CONFIRM_TRANSACTION;
        G_context.signing_type = type;
        G_context.tx_info.clear_signing_available = false;
        G_context.state = STATE_EXPECTING_MORE;

        if (!buffer_read_u8(cdata, &G_context.bip32_path_len) ||
            !buffer_read_bip32_path(cdata,
                                    G_context.bip32_path,
                                    (size_t) G_context.bip32_path_len)) {
            return SW_WRONG_DATA_LENGTH;
        }

        // Initialize transaction context after reading BIP32 path
        // some initialization functions need the BIP32 path
        // (e.g. to derive the public key)

        if (type == SIGN_PREPARED_TRANSACTION) {
            process_prepared_tx_init();
        } else if (type == SIGN_UNTYPED_VERSIONED_MESSAGE) {
            if (!process_untyped_versioned_msg_tx_init(cdata)) {
                return SW_WRONG_DATA_LENGTH;
            }
        }

    } else {  // parse transaction
        if (G_context.req_type != CONFIRM_TRANSACTION) {
            PRINTF("Request type mismatch: expected CONFIRM_TRANSACTION, got %d\n",
                   G_context.req_type);
            return SW_BAD_STATE;
        }

        if (G_context.tx_info.raw_tx_len + cdata->size > MAX_TRANSACTION_LEN) {
            return SW_WRONG_TX_LENGTH;
        }

        if (!buffer_move(cdata,
                         G_context.tx_info.raw_tx + G_context.tx_info.raw_tx_len,
                         cdata->size)) {
            PRINTF("Failed to copy transaction chunk\n");
            return SW_TX_PARSING_FAIL;
        }

        G_context.tx_info.raw_tx_len += cdata->size;

        if (msg_end && more) {
            PRINTF("TX CHUNK : Message complete, more expected\n");
            buf.ptr = G_context.tx_info.raw_tx, buf.size = G_context.tx_info.raw_tx_len,
            buf.offset = 0;
            // Reset for next message (transaction part)
            G_context.tx_info.raw_tx_len = 0;
            G_context.state = STATE_MSG_COMPLETE;
        } else if (msg_end) {
            buf.ptr = G_context.tx_info.raw_tx, buf.size = G_context.tx_info.raw_tx_len,
            buf.offset = 0;
            PRINTF("TX CHUNK : All transaction data received\n");
            G_context.state = STATE_PARSED;
        } else if (more) {
            G_context.state = STATE_EXPECTING_MORE;
        } else {
            PRINTF("Invalid state: neither more nor msg_end is set\n");
            return SW_BAD_STATE;
        }
    }
    return 0;
}

static MUST_CHECK int process_transaction_hash(buffer_t *buffer) {
    LEDGER_ASSERT(buffer != NULL, "buffer is NULL");
    if (G_context.state != STATE_PARSED) {
        PRINTF("Invalid state: expected STATE_PARSED, got %d\n", G_context.state);
        return SW_BAD_STATE;
    }

    // Hash length should either be 32 bytes (SHA-256) or 34 bytes (2 prefix bytes + SHA-256)
    if (buffer->size != 32 && buffer->size != 34) {
        PRINTF("Invalid hash length: expected 32 or 34, got %d\n", buffer->size);
        return SW_WRONG_DATA_LENGTH;
    }

    memcpy(G_context.tx_info.m_hash, buffer->ptr, buffer->size);
    G_context.tx_info.m_hash_len = (uint8_t) buffer->size;

    return 0;
}
