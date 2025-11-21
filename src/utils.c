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
#include <string.h>   // memmove

#include "types.h"
#include "utils.h"

#include "os.h"
#include "cx.h"

#define SHA256_ALGO_PREFIX ((uint8_t) 0x12)
#define SHA256_ALGO_LENGTH ((uint8_t) 0x20)

void canton_hash(uint8_t purpose,
                 const uint8_t *data,
                 size_t data_len,
                 uint8_t out[CANTON_HASH_LEN]) {
    LEDGER_ASSERT(out != NULL, "NULL out pointer passed to canton_hash");
    LEDGER_ASSERT(data != NULL || data_len == 0,
                  "NULL data with non-zero length passed to canton_hash");
    cx_sha256_t ctx;
    uint8_t purpose_be[UINT32_T_LEN] = {0, 0, 0, purpose};
    uint8_t tmp[SHA256_HASH_LEN] = {0};
    CX_ASSERT(cx_sha256_init_no_throw(&ctx));
    CX_ASSERT(cx_hash_update((cx_hash_t *) &ctx, purpose_be, sizeof(purpose_be)));
    if (data_len > 0) {
        CX_ASSERT(cx_hash_update((cx_hash_t *) &ctx, data, data_len));
    }
    CX_ASSERT(cx_hash_final((cx_hash_t *) &ctx, tmp));
    out[0] = SHA256_ALGO_PREFIX;
    out[1] = SHA256_ALGO_LENGTH;
    memmove(out + 2, tmp, SHA256_HASH_LEN);
}

/* -------------------------------------------------------------------------- */
/*  Helper functions                                                          */
/* -------------------------------------------------------------------------- */

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

MUST_CHECK int atoint(const char *str) {
    LEDGER_ASSERT(str != NULL, "NULL string pointer passed to atoint");

    int res = 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (!is_digit(str[i])) {
            return 0;
        }
        res = res * 10 + str[i] - '0';
    }

    return res;
}

MUST_CHECK uint64_t atoull(const char *str) {
    LEDGER_ASSERT(str != NULL, "NULL string pointer passed to atoull");

    uint64_t res = 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (!is_digit(str[i])) {
            return 0;
        }
        res = res * 10 + str[i] - '0';
    }

    return res;
}
