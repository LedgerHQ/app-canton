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
#include <string.h>   // memset

#include "os.h"
#include "glyphs.h"
#include "os_io_seproxyhal.h"
#include "nbgl_use_case.h"
#include "io.h"
#include "bip32.h"
#include "format.h"

#include "display.h"
#include "constants.h"
#include "globals.h"
#include "sw.h"
#include "validate.h"
#include "tx_types.h"
#include "menu.h"
#include "utils.h"

#define BLIND_SIGN_PAIR_LIST_NB 1
static nbgl_contentTagValueList_t pairList;

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void review_choice(bool confirm) {
    // Answer, display a status page and go back to main
    validate_transaction(confirm);
    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
    } else {
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_menu_main);
    }
}

// Public function to start the transaction review
// - Check if the app is in the right state for transaction review
// - Format the amount and address strings in g_amount and g_address buffers
// - Display the first screen of the transaction review
// - Display a warning if the transaction is blind-signed
MUST_CHECK int ui_display_transaction_bs_choice(bool is_blind_signed) {
    if (G_context.req_type != CONFIRM_TRANSACTION || G_context.state != STATE_PARSED) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    if (is_blind_signed) {
        PRINTF("Hash: %.*H\n", sizeof(G_context.tx_info.m_hash), G_context.tx_info.m_hash);
        // Setup data to display
        size_t hex_hash_length = 2 * G_context.tx_info.m_hash_len + 1;
        G_context.tx_info.pairs =
            (nbgl_contentTagValue_t *) app_mem_alloc(sizeof(nbgl_contentTagValue_t));
        LEDGER_ASSERT(G_context.tx_info.pairs != NULL, "Memory full");
        memset(G_context.tx_info.pairs, 0, sizeof(nbgl_contentTagValue_t));
        G_context.tx_info.pairs[0].item = "Transaction hash";
        G_context.tx_info.pairs[0].value = (char *) app_mem_alloc(hex_hash_length);
        G_context.tx_info.pairs_count = BLIND_SIGN_PAIR_LIST_NB;
        LEDGER_ASSERT(G_context.tx_info.pairs[0].value != NULL, "Memory full");
        SNPRINTF((char *) G_context.tx_info.pairs[0].value,
                 hex_hash_length,
                 "%.*H",
                 G_context.tx_info.m_hash_len,
                 G_context.tx_info.m_hash);

        // Setup list
        pairList.nbMaxLinesForValue = 0;
        pairList.nbPairs = BLIND_SIGN_PAIR_LIST_NB;
        pairList.pairs = G_context.tx_info.pairs;

        // Start blind-signing review flow
        nbgl_useCaseReviewBlindSigning(TYPE_TRANSACTION,
                                       &pairList,
                                       &ICON_APP_CANTON,
                                       "Review transaction hash",
                                       NULL,
#ifdef SCREEN_SIZE_WALLET
                                       "Accept risk and sign\ntransaction?",
#else
                                       NULL,
#endif
                                       NULL,
                                       review_choice);
    } else {
        pairList.nbPairs = G_context.tx_info.pairs_count;
        pairList.pairs = G_context.tx_info.pairs;

        PRINTF("Pair count: %d\n", pairList.nbPairs);
        // Print all pairs for debugging
        for (size_t i = 0; i < pairList.nbPairs; i++) {
            PRINTF("Pair %d: %s: %s\n", i, pairList.pairs[i].item, pairList.pairs[i].value);
        }

        PRINTF("Hash : %.*H\n", G_context.tx_info.m_hash_len, G_context.tx_info.m_hash);

        // Start review flow
        nbgl_useCaseReview(TYPE_TRANSACTION,
                           &pairList,
                           &ICON_APP_CANTON,
                           G_context.tx_info.review_title,
                           NULL,
                           G_context.tx_info.review_finish,
                           review_choice);
    }
    return 0;
}

// Flow used to display a blind-signed transaction
MUST_CHECK int ui_display_blind_signed_transaction(void) {
    return ui_display_transaction_bs_choice(true);
}

// Flow used to display a clear-signed transaction
MUST_CHECK int ui_display_transaction() {
    return ui_display_transaction_bs_choice(false);
}
