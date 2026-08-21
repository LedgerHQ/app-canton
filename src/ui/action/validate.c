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

#include <stdbool.h>  // bool

#include "crypto_helpers.h"

#include "validate.h"
#include "menu.h"
#include "sw.h"
#include "globals.h"
#include "send_response.h"
#include "pb_node_display_parser.h"

void validate_pubkey(bool choice) {
    if (choice) {
        LEDGER_ASSERT(helper_send_response_pubkey() >= 0, "Failed to send pubkey response");
    } else {
        io_send_sw(SW_DENY);
    }
}

static int crypto_sign_message(void) {
    uint32_t info = 0;
    size_t sig_len = sizeof(G_context.tx_info.signature);

    explicit_bzero(G_context.tx_info.signature, sizeof(G_context.tx_info.signature));

    cx_err_t error = bip32_derive_with_seed_eddsa_sign_hash_256(HDW_ED25519_SLIP10,
                                                                CX_CURVE_Ed25519,
                                                                G_context.bip32_path,
                                                                G_context.bip32_path_len,
                                                                CX_SHA512,
                                                                G_context.tx_info.m_hash,
                                                                G_context.tx_info.m_hash_len,
                                                                G_context.tx_info.signature,
                                                                &sig_len,
                                                                NULL,
                                                                0);

    if (error != CX_OK) {
        return -1;
    }

    PRINTF("Signature: %.*H\n", sig_len, G_context.tx_info.signature);

    if (sig_len != ED25519_SIG_LEN) {
        PRINTF("Invalid signature length: %d\n", sig_len);
        return -1;
    }

    G_context.tx_info.signature_len = sig_len;
    G_context.tx_info.v = (uint8_t) (info & CX_ECCINFO_PARITY_ODD);

    return 0;
}

void validate_transaction(bool choice) {
    if (choice) {
        G_context.state = STATE_APPROVED;

        if (crypto_sign_message() != 0) {
            G_context.state = STATE_NONE;
            io_send_sw(SW_SIGNATURE_FAIL);
        } else {
            LEDGER_ASSERT(helper_send_response_sig() >= 0, "Failed to send signature response");
        }
    } else {
        G_context.state = STATE_NONE;
        io_send_sw(SW_DENY);
    }
    // Frees any remaining allocated memory for display items
    cleanup_display_items();
}
