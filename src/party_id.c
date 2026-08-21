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
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include <string.h>

#include "os.h"
#include "cx.h"
#include "ledger_assert.h"

#include "party_id.h"
#include "tx_types.h"

MUST_CHECK bool party_id_from_pubkey(const uint8_t public_key[ED25519_RAW_PUBLIC_KEY_LEN],
                                     uint8_t *out,
                                     size_t out_len) {
    uint8_t tmp[CANTON_HASH_LEN] = {0};

    LEDGER_ASSERT(out != NULL, "NULL out");

    if (out_len < PARTY_ID_LEN) {
        return false;
    }

    canton_hash(0x0C, public_key, ED25519_RAW_PUBLIC_KEY_LEN, tmp);
    // Format party id as hex(public_key) : hex(sha256(hash purpose (12) || public_key))
    SNPRINTF((char *) out, out_len, "ldg::%.*h", sizeof(tmp), tmp);

    return true;
}
