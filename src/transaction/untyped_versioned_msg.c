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
#include <stdlib.h>  // qsort

#include "mem.h"
#include "os.h"
#include "cx.h"
#include "cx_errors.h"
#include "ledger_assert.h"
#include "globals.h"

#include "party_id.h"
#include "utils.h"
#include "buffer.h"
#include "tx_types.h"
#include "sw.h"
#include "bytewriter.h"
#include "pb_parser.h"
#include "pb_node_display_parser.h"
#include "get_public_key.h"
#include "send_response.h"
#include "party_id.h"
#include "crypto_data.h"
#include "constants.h"

#define HASH_LEN                                     34
#define HEX_LEN                                      (HASH_LEN * 2 + 1)
#define MAX_HASHES                                   3  // Adjust as needed
#define PURPOSE_TOPOLOGY_TRANSACTION_SIGNATURE       ((uint8_t) 11)
#define PURPOSE_MULTI_TOPOLOGY_TRANSACTION_SIGNATURE ((uint8_t) 55)
#define ONBOARDING_FLOW_DISPLAY_FIELDS_NB            4  // Max number of display fields for onboarding flow
#define CHALLENGE_AND_DEADLINE_LEN                   24  // 16 bytes challenge + 8 bytes deadline
#define ED25519_RAW_KEY_LEN                          32
#define ED25519_DER_KEY_LEN                          44
#define ED25519_DER_PREFIX_LEN                       12

static const uint8_t ED25519_DER_PREFIX[ED25519_DER_PREFIX_LEN] =
    {0x30, 0x2A, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, 0x03, 0x21, 0x00};

// Separate const config from mutable state
typedef struct {
    const char *item_name;
    bool mandatory;
} field_config_t;

typedef struct {
    const field_config_t *config;  // Pointer to const config
    bool found;                    // Mutable state
} field_state_t;

typedef struct {
    const char *participant_id;
    const char *participant_name;
} participant_id_to_name_mapping_t;

typedef struct {
    const participant_id_to_name_mapping_t *mappings;
    size_t count;
} valid_participants_config_t;

#define PARTY_FIELD_IDX         0
#define PARTICIPANT_1_FIELD_IDX 1
#define PARTICIPANT_2_FIELD_IDX 2
#define MAX_PARTICIPANTS_NB     2
#define THRESHOLD_FIELD_IDX     3

const participant_id_to_name_mapping_t MAINNET_SINGLE_VALIDATOR[] = {
    {"ledger-ledgerops-2::12207a4859ad414f4f47c2d773ddf4ea88de8c3a1aab19abaa197e504acdbf679d3c",
     "Ledger Validator"},
};

const participant_id_to_name_mapping_t MAINNET_ARQITECH_SINGLE_VALIDATOR[] = {
    {"validator-MPCH-ARQI-1::"
     "1220a9be0f658dfc64deb70a2818b460315dc1815f8f9b4ec1017c4432910dd375fe",
     "Arqitech Validator"},
};

const participant_id_to_name_mapping_t MAINNET_DUAL_VALIDATORS[] = {
    {"ledger-ledgerops-2::12207a4859ad414f4f47c2d773ddf4ea88de8c3a1aab19abaa197e504acdbf679d3c",
     "Ledger Validator"},
    {"Ledger-Kiln-2::1220e2225d5a297fae4000be2e3ca560ce802461da04c8e3e40c9fbbf0547f4fe8e3",
     "Kiln Validator"},
};

const participant_id_to_name_mapping_t TESTNET_SINGLE_VALIDATOR[] = {
    {"ledger-ledgeropstestnet-0::"
     "122095f38f5c73cc18fbeb3290f8c17f7a1ff190f66fe159c671cf1fb0dc634eedaf",
     "Ledger Testnet\nValidator"},
};

const participant_id_to_name_mapping_t TESTNET_ARQITECH_SINGLE_VALIDATOR[] = {
    {"validator-MPCH-ARQI-1::"
     "122012c83fd018b14f91b426181fb2171e5a896eadffbbe0993d440613ccf23baf45",
     "Arqitech Testnet\nValidator"},
};

const participant_id_to_name_mapping_t TESTNET_DUAL_VALIDATORS[] = {
    {"ledger-ledgeropstestnet-0::"
     "122095f38f5c73cc18fbeb3290f8c17f7a1ff190f66fe159c671cf1fb0dc634eedaf",
     "Ledger Testnet\nValidator"},
    {"Ledger-KilnTestnet-2::"
     "1220fa9df3caa84092023bf7edf28de1d28f96caf9b7d130385bfe6e284be6e0fbd7",
     "Kiln Testnet\nValidator"},
};

const participant_id_to_name_mapping_t DEVNET_SINGLE_VALIDATOR[] = {
    {"ledger-ledgeropsdevnet-0::"
     "12208f74f551f8c28b68414fc3bb4b8466178055845485878a1af8ac1fe96f88fad2",
     "Ledger Devnet\nValidator"},
};

const participant_id_to_name_mapping_t DEVNET_ARQITECH_SINGLE_VALIDATOR[] = {
    {"validator-MPCH-ARQI-2::"
     "1220625b353913e8b722ca74a6675bb8a19f12c5aa713e51e3090730fce8e2dcbaae",
     "Arqitech Devnet\nValidator"},
};

const participant_id_to_name_mapping_t DEVNET_DUAL_VALIDATORS[] = {
    {"ledger-ledgeropsdevnet-0::"
     "12208f74f551f8c28b68414fc3bb4b8466178055845485878a1af8ac1fe96f88fad2",
     "Ledger Devnet\nValidator"},
    {"Ledger-KilnDevnet-2::12203b77e5d74eb787ff0251fd76949379a625368646302a203fea7f7db1dd5402bf",
     "Kiln Devnet\nValidator"},
};

const valid_participants_config_t VALID_PARTICIPANTS_CONFIGS[] = {
    {MAINNET_SINGLE_VALIDATOR,
     sizeof(MAINNET_SINGLE_VALIDATOR) / sizeof(MAINNET_SINGLE_VALIDATOR[0])},
    {MAINNET_ARQITECH_SINGLE_VALIDATOR,
     sizeof(MAINNET_ARQITECH_SINGLE_VALIDATOR) / sizeof(MAINNET_ARQITECH_SINGLE_VALIDATOR[0])},
    {MAINNET_DUAL_VALIDATORS, sizeof(MAINNET_DUAL_VALIDATORS) / sizeof(MAINNET_DUAL_VALIDATORS[0])},
    {TESTNET_SINGLE_VALIDATOR,
     sizeof(TESTNET_SINGLE_VALIDATOR) / sizeof(TESTNET_SINGLE_VALIDATOR[0])},
    {TESTNET_ARQITECH_SINGLE_VALIDATOR,
     sizeof(TESTNET_ARQITECH_SINGLE_VALIDATOR) / sizeof(TESTNET_ARQITECH_SINGLE_VALIDATOR[0])},
    {TESTNET_DUAL_VALIDATORS, sizeof(TESTNET_DUAL_VALIDATORS) / sizeof(TESTNET_DUAL_VALIDATORS[0])},
    {DEVNET_SINGLE_VALIDATOR, sizeof(DEVNET_SINGLE_VALIDATOR) / sizeof(DEVNET_SINGLE_VALIDATOR[0])},
    {DEVNET_ARQITECH_SINGLE_VALIDATOR,
     sizeof(DEVNET_ARQITECH_SINGLE_VALIDATOR) / sizeof(DEVNET_ARQITECH_SINGLE_VALIDATOR[0])},
    {DEVNET_DUAL_VALIDATORS, sizeof(DEVNET_DUAL_VALIDATORS) / sizeof(DEVNET_DUAL_VALIDATORS[0])},
};

// Const configurations (stored in flash)
const field_config_t PARTY_FIELD_CONFIG = {"Add account", true};
static const char *SINGLE_VALIDATOR_LABEL = "Associate to validator";
const field_config_t PARTICIPANT_1_UID_FIELD_CONFIG = {"Associate to validator 1", true};
const field_config_t PARTICIPANT_2_UID_FIELD_CONFIG = {"Associate to validator 2", false};
const field_config_t THRESHOLD_FIELD_CONFIG = {"Validators threshold", false};

static const field_config_t *const
    ONBOARDING_FLOW_DISPLAY_CONFIGS[ONBOARDING_FLOW_DISPLAY_FIELDS_NB] = {
        &PARTY_FIELD_CONFIG,
        &PARTICIPANT_1_UID_FIELD_CONFIG,
        &PARTICIPANT_2_UID_FIELD_CONFIG,
        &THRESHOLD_FIELD_CONFIG,
};

// Mutable state array (stored in RAM)
static field_state_t field_states[ONBOARDING_FLOW_DISPLAY_FIELDS_NB];

static bool challenge_and_deadline_parsed = false;
static uint8_t challenge_and_deadline[CHALLENGE_AND_DEADLINE_LEN] = {0};

static uint8_t (*tx_hashes)[HASH_LEN] = NULL;
static size_t hash_count = 0;
static bool has_parsed_namespace_delegation = false;
static bool has_parsed_party_to_participant = false;
static bool has_parsed_party_to_key_mapping = false;

static const char *REVIEW_TITLE = "Review transaction to add account";
static const char *REVIEW_FINISH = "Sign transaction to add account?";

static int parse_topology_transaction_for_display(buffer_t *buf);

static void init_hash_storage(void) {
    if (tx_hashes) {
        app_mem_free(tx_hashes);
    }
    tx_hashes = app_mem_alloc(MAX_HASHES * sizeof(uint8_t[HASH_LEN]));
    LEDGER_ASSERT(tx_hashes != NULL, "Failed to allocate memory for hash storage");
    hash_count = 0;
}

static void cleanup_hash_storage() {
    if (tx_hashes) {
        app_mem_free(tx_hashes);
        tx_hashes = NULL;
        hash_count = 0;
    }
}

static void add_hash(const uint8_t hash[HASH_LEN]) {
    LEDGER_ASSERT(hash != NULL, "NULL hash");
    LEDGER_ASSERT(tx_hashes != NULL, "Hash storage not initialized");
    LEDGER_ASSERT(hash_count < MAX_HASHES, "Hash storage full");
    memcpy(tx_hashes[hash_count], hash, HASH_LEN);
    hash_count++;
}

static int compare_hashes_hex(const void *a, const void *b) {
    const uint8_t *hash_a = (const uint8_t *) a;
    const uint8_t *hash_b = (const uint8_t *) b;

    char hex_a[HEX_LEN];
    char hex_b[HEX_LEN];

    SNPRINTF(hex_a, sizeof(hex_a), "%.*h", HASH_LEN, hash_a);
    SNPRINTF(hex_b, sizeof(hex_b), "%.*h", HASH_LEN, hash_b);
    return strcmp(hex_a, hex_b);
}

MUST_CHECK static bool read_challenge_and_deadline(buffer_t *cdata) {
    // Check challenge presence
    if (!buffer_can_read(cdata, 1)) {
        return true;  // Challenge not present, not an error
    }

    // Check the challenge + deadline length matches expected length
    uint8_t length;
    if (!buffer_read_u8(cdata, &length) || length != CHALLENGE_AND_DEADLINE_LEN) {
        return false;
    }

    // Read challenge + deadline
    if (!buffer_can_read(cdata, CHALLENGE_AND_DEADLINE_LEN) ||
        !buffer_move(cdata, challenge_and_deadline, CHALLENGE_AND_DEADLINE_LEN)) {
        return false;
    }

    challenge_and_deadline_parsed = true;
    return true;
}

MUST_CHECK bool process_untyped_versioned_msg_tx_init(buffer_t *cdata) {
    LEDGER_ASSERT(cdata != NULL, "Null buf in process_untyped_versioned_msg_tx_init");

    uint8_t chain_code[MAX_CHAINCODE_LEN] = {0};
    init_hash_storage();
    LEDGER_ASSERT(
        init_transaction_pairs(&G_context.tx_info, ONBOARDING_FLOW_DISPLAY_FIELDS_NB) == true,
        "Failed to initialize transaction pairs");
    G_context.tx_info.pairs_count = 0;
    has_parsed_namespace_delegation = false;
    has_parsed_party_to_participant = false;
    has_parsed_party_to_key_mapping = false;
    challenge_and_deadline_parsed = false;

    // Reuse the signature field to store the derived public key
    // using G_context.pk_info.raw_public_key could result in corrupted value
    // since pk_info is a union shared with tx_info.
    cx_err_t error = derive_public_key(G_context.bip32_path,
                                       G_context.bip32_path_len,
                                       G_context.tx_info.signature,
                                       chain_code);

    LEDGER_ASSERT(error == CX_OK, "Failed to derive public key");

    // Initialize field states
    for (size_t i = 0; i < ONBOARDING_FLOW_DISPLAY_FIELDS_NB; i++) {
        field_states[i].config = (const field_config_t *) PIC(ONBOARDING_FLOW_DISPLAY_CONFIGS[i]);
        field_states[i].found = false;
    }

    // Read optional challenge and deadline
    return read_challenge_and_deadline(cdata);
}

static void compute_multi_hash(void) {
    // Allocate storage for a concatenated string of all hashes + their lengths
    size_t len = hash_count * HASH_LEN + hash_count * 4 +
                 4;  // Each hash prefixed by its length (4 bytes) + 4 bytes for count
    uint8_t *concat = app_mem_alloc(len);
    ByteWriter bw;
    bw_init(&bw, concat, len);
    bw_put_u32_be(&bw, hash_count);  // Prefix with number of hashes
    // Sort hashes lexicographically in hex format
    qsort(tx_hashes, hash_count, HASH_LEN, compare_hashes_hex);
    for (size_t i = 0; i < hash_count; i++) {
        // Concatenate each hash, prefixed them with their length (always 34)
        bw_put_u32_be(&bw, HASH_LEN);
        bw_put(&bw, tx_hashes[i], HASH_LEN);
    }
    // Compute final hash
    canton_hash(PURPOSE_MULTI_TOPOLOGY_TRANSACTION_SIGNATURE,
                concat,
                len,
                G_context.tx_info.m_hash);
    G_context.tx_info.m_hash_len = HASH_LEN;
    PRINTF("Final untyped versioned message hash: %.*H\n", HASH_LEN, G_context.tx_info.m_hash);
    app_mem_free(concat);
}

static MUST_CHECK int sign_challenge(void) {
    size_t sig_len = sizeof(G_context.tx_info.challenge_signature);
    uint8_t data_to_sign[HASH_LEN + CHALLENGE_AND_DEADLINE_LEN];
    size_t size;

    // Initialize private key structure
    cx_ecfp_256_private_key_t privkey = {.curve = CX_CURVE_Ed25519, .d_len = 32};
    memcpy(privkey.d, ATTESTATION_KEY, 32);

    // Prepare data to sign: multi-hash + challenge + deadline
    memcpy(data_to_sign, G_context.tx_info.m_hash, HASH_LEN);
    memcpy(data_to_sign + HASH_LEN, challenge_and_deadline, CHALLENGE_AND_DEADLINE_LEN);

    // Sign the data
    explicit_bzero(G_context.tx_info.challenge_signature,
                   sizeof(G_context.tx_info.challenge_signature));
    CX_ASSERT(cx_eddsa_sign_no_throw(&privkey,
                                     CX_SHA512,
                                     data_to_sign,
                                     sizeof(data_to_sign),
                                     G_context.tx_info.challenge_signature,
                                     sig_len));
    CX_ASSERT(cx_ecdomain_parameters_length(CX_CURVE_Ed25519, &size));
    sig_len = size * 2;  // r and s each of size 'size'

    if (sig_len != ED25519_SIG_LEN) {
        PRINTF("Invalid challenge signature length: %d\n", sig_len);
        return -1;
    }

    PRINTF("Challenge signature: %.*H\n", sig_len, G_context.tx_info.challenge_signature);

    // Set signature length and flag
    G_context.tx_info.challenge_signature_len = (uint8_t) sig_len;
    G_context.tx_info.has_challenge_signature = true;

    return 0;
}

MUST_CHECK int process_untyped_versioned_msg_tx(buffer_t *buf) {
    UNUSED(buf);
    uint8_t h[HASH_LEN] = {0};

    canton_hash(PURPOSE_TOPOLOGY_TRANSACTION_SIGNATURE, buf->ptr, buf->size, h);
    add_hash(h);

    int ret = parse_topology_transaction_for_display(buf);
    if (ret != 0) {
        return ret;
    }

    if (G_context.state == STATE_PARSED) {
        compute_multi_hash();
        cleanup_hash_storage();

        if (challenge_and_deadline_parsed) {
            // Sign multihash + challenge + deadline
            if (sign_challenge() != 0) {
                return SW_CHALLENGE_SIGNATURE_FAIL;
            }
        }

        if (has_parsed_namespace_delegation && has_parsed_party_to_key_mapping &&
            has_parsed_party_to_participant) {
            // Allow signing only if all required fields have been parsed
            for (size_t i = 0; i < ONBOARDING_FLOW_DISPLAY_FIELDS_NB; i++) {
                if (field_states[i].config->mandatory && !field_states[i].found) {
                    PRINTF("Mandatory field %s not found\n",
                           (char *) PIC(field_states[i].config->item_name));
                    return SW_TOPOLOGY_MANDATORY_FIELD_MISSING;
                }
            }

            G_context.tx_info.clear_signing_available = true;
            // Allocate review title and finish strings
            G_context.tx_info.review_title = REVIEW_TITLE;
            G_context.tx_info.review_finish = REVIEW_FINISH;
        }
    }
    return 0;
}

// Helper function to set field value
MUST_CHECK static bool set_field_value(transaction_ctx_t *tx_info,
                                       size_t field_idx,
                                       const char *value) {
    LEDGER_ASSERT(tx_info != NULL, "Null tx_ctx in set_field_value");
    LEDGER_ASSERT(value != NULL, "Null value passed to set_field_value");

    uint8_t idx = tx_info->pairs_count;

    // Input validation
    if (idx >= ONBOARDING_FLOW_DISPLAY_FIELDS_NB ||
        field_idx >= ONBOARDING_FLOW_DISPLAY_FIELDS_NB || value == NULL) {
        return false;
    }

    size_t value_len = strlen(value) + 1;

    // Allocate and set value
    tx_info->pairs[idx].value = (char *) app_mem_alloc(value_len);
    if (tx_info->pairs[idx].value == NULL) {
        return false;  // Memory allocation failed
    }

    memcpy((void *) tx_info->pairs[idx].value, value, value_len);
    tx_info->pairs[idx].item = (char *) PIC(field_states[field_idx].config->item_name);
    tx_info->pairs_count++;

    // Mark field as found
    field_states[field_idx].found = true;

    return true;
}

static int check_party_key_value(const uint8_t *key_to_check_bytes,
                                 size_t key_to_check_len,
                                 CryptoKeyFormat key_format) {
    if (key_to_check_bytes == NULL || key_to_check_len == 0) {
        return SW_TOPOLOGY_MISSING_PARTY_KEY;
    }

    PRINTF("Key to check: %.*H\n", key_to_check_len, key_to_check_bytes);
    PRINTF("Derived public key: %.*H\n", PUBKEY_LEN, G_context.tx_info.signature);

    switch (key_format) {
        case CRYPTO_KEY_FORMAT_RAW:
            // Raw Ed25519 public key
            if (key_to_check_len != ED25519_RAW_KEY_LEN) {
                return SW_TOPOLOGY_PARTY_KEY_WRONG_FORMAT;
            }
            if (memcmp(key_to_check_bytes, G_context.tx_info.signature, ED25519_RAW_KEY_LEN) != 0) {
                return SW_TOPOLOGY_PARTY_KEY_MISMATCH;
            }
            break;
        case CRYPTO_KEY_FORMAT_DER_X509:
            // DER-encoded (RFC 8410 / RFC 5280)
            if (key_to_check_len != ED25519_DER_KEY_LEN) {
                return SW_TOPOLOGY_PARTY_KEY_WRONG_FORMAT;
            }
            if (memcmp(key_to_check_bytes, ED25519_DER_PREFIX, ED25519_DER_PREFIX_LEN) != 0) {
                return SW_TOPOLOGY_PARTY_KEY_WRONG_FORMAT;
            }
            if (memcmp(key_to_check_bytes + ED25519_DER_PREFIX_LEN,
                       G_context.tx_info.signature,
                       ED25519_RAW_KEY_LEN) != 0) {
                return SW_TOPOLOGY_PARTY_KEY_MISMATCH;
            }
            break;
        default:
            return SW_TOPOLOGY_PARTY_KEY_WRONG_FORMAT;
    }
    return 0;
}

MUST_CHECK static bool check_party_id_value(const char *party_id) {
    if (party_id == NULL) {
        return false;
    }

    // Check against derived party id
    uint8_t derived_party_id[PARTY_ID_LEN] = {0};
    if (!party_id_from_pubkey(G_context.tx_info.signature,
                              derived_party_id,
                              sizeof(derived_party_id))) {
        return false;
    }

    if (strcmp(party_id, (const char *) derived_party_id) == 0) {
        return true;
    } else {
        return false;
    }
}

// Process namespace delegation mapping
static int process_namespace_delegation(const NamespaceDelegation *delegation,
                                        transaction_ctx_t *tx_info) {
    UNUSED(tx_info);
    LEDGER_ASSERT(!has_parsed_namespace_delegation, "Multiple namespace delegations found");
    LEDGER_ASSERT(delegation != NULL, "NULL namespace delegation");

    int ret = 0;

    if (delegation->has_target_key) {
        // Check key value against derived public key
        ret = check_party_key_value(delegation->target_key.public_key.bytes,
                                    delegation->target_key.public_key.size,
                                    delegation->target_key.format);
    } else {
        ret = SW_TOPOLOGY_MISSING_TARGET_KEY;
    }

    has_parsed_namespace_delegation = true;
    return ret;
}

// Process party to key mapping
static int process_party_to_key_mapping(const PartyToKeyMapping *mapping,
                                        transaction_ctx_t *tx_info) {
    UNUSED(tx_info);
    LEDGER_ASSERT(!has_parsed_party_to_key_mapping, "Multiple party to key mappings found");
    LEDGER_ASSERT(mapping != NULL, "NULL party to key mapping");

    int ret = 0;
    // Set party to key mapping specific fields
    if (!check_party_id_value(mapping->party)) {
        return SW_TOPOLOGY_PARTY_ID_MISMATCH;
    }

    // For signing keys, we'll show the first one or count if multiple
    if (mapping->signing_keys_count > 0) {
        com_digitalasset_canton_crypto_v30_SigningPublicKey *key = &mapping->signing_keys[0];
        // Check key value against derived public key
        ret = check_party_key_value(key->public_key.bytes, key->public_key.size, key->format);
    } else {
        ret = SW_TOPOLOGY_NO_SIGNING_KEYS;
    }

    has_parsed_party_to_key_mapping = true;
    return ret;
}

// Process party to participant mapping
static int process_party_to_participant(const PartyToParticipant *mapping,
                                        transaction_ctx_t *tx_info) {
    LEDGER_ASSERT(!has_parsed_party_to_participant, "Multiple party to participant mappings found");
    LEDGER_ASSERT(tx_info != NULL, "NULL tx_ctx in process_party_to_participant");

    // Pre-checks and mandatory count checks
    if (mapping->party == NULL) {
        return SW_TOPOLOGY_MISSING_PARTY;
    }

    if (!check_party_id_value(mapping->party)) {
        return SW_TOPOLOGY_PARTY_ID_MISMATCH;
    }

    if (mapping->participants_count > MAX_PARTICIPANTS_NB) {
        return SW_TOPOLOGY_UNEXPECTED_NUMBER_OF_PARTICIPANTS;
    }

    if (mapping->threshold != mapping->participants_count) {
        return SW_TOPOLOGY_UNEXPECTED_THRESHOLD_VALUE;
    }

    if (mapping->participants_count > 0 && mapping->participants == NULL) {
        return SW_TOPOLOGY_MISSING_PARTICIPANT_DATA;
    }

    // Set party field
    LEDGER_ASSERT(set_field_value(tx_info, PARTY_FIELD_IDX, mapping->party) == true,
                  "Failed to set party field");

    size_t configs_count =
        sizeof(VALID_PARTICIPANTS_CONFIGS) / sizeof(VALID_PARTICIPANTS_CONFIGS[0]);
    size_t pc = mapping->participants_count;
    uint8_t found_valid = 0;
    bool matched_valid[MAX_PARTICIPANTS_NB] = {false};
    char *participant_names[MAX_PARTICIPANTS_NB] = {NULL};

    for (size_t i = 0; i < configs_count; i++) {
        found_valid = 0;
        memset(matched_valid, 0, sizeof(matched_valid));
        memset(participant_names, 0, sizeof(participant_names));

        valid_participants_config_t config = VALID_PARTICIPANTS_CONFIGS[i];

        if (config.count == pc) {
            for (size_t j = 0; j < pc; j++) {
                const char *uid = (const char *) PIC(mapping->participants[j].participant_uid);
                if (uid == NULL || *uid == '\0') {
                    return SW_TOPOLOGY_MISSING_PARTICIPANT_DATA;
                }
                for (size_t k = 0; k < config.count; k++) {
                    participant_id_to_name_mapping_t *mappings =
                        (participant_id_to_name_mapping_t *) PIC(config.mappings);
                    const char *valid_id = (const char *) PIC(mappings[k].participant_id);
                    const char *valid_name = (const char *) PIC(mappings[k].participant_name);
                    if (strcmp(uid, valid_id) == 0) {
                        if (matched_valid[k]) {
                            return SW_TOPOLOGY_UNEXPECTED_DUPLICATE_PARTICIPANT;
                        }
                        matched_valid[k] = true;
                        found_valid++;
                        participant_names[j] = (char *) valid_name;
                        break;
                    }
                }
            }
        }
        if (found_valid == pc) {
            break;  // Found a matching config, no need to check further
        }
    }

    // Final check for missing mandatory participants
    if (found_valid != pc) {
        return SW_TOPOLOGY_UNEXPECTED_PARTICIPANT_ID;
    }

    // Set participant name fields
    for (size_t j = 0; j < pc; j++) {
        size_t field_idx = (j == 0) ? PARTICIPANT_1_FIELD_IDX : PARTICIPANT_2_FIELD_IDX;
        LEDGER_ASSERT(set_field_value(tx_info, field_idx, participant_names[j]) == true,
                      "Failed to set participant name field");
    }

    // When only one participant, use singular label instead of "Associate to validator 1"
    if (pc == 1) {
        tx_info->pairs[tx_info->pairs_count - 1].item = (char *) PIC(SINGLE_VALIDATOR_LABEL);
    }

    // Set threshold field
    if (mapping->threshold > 1) {
        char threshold_str[DEFAULT_DECODE_BUFFER_SIZE];
        SNPRINTF(threshold_str,
                 sizeof(threshold_str),
                 "%u out of %u",
                 mapping->threshold,
                 mapping->participants_count);

        LEDGER_ASSERT(set_field_value(tx_info, THRESHOLD_FIELD_IDX, threshold_str) == true,
                      "Failed to set threshold field");
    }

    has_parsed_party_to_participant = true;
    return 0;
}

static int parse_topology_transaction_for_display(buffer_t *buf) {
    LEDGER_ASSERT(buf != NULL, "NULL buf in parse_topology_tx_for_display");

    int32_t ret = 0;
    parser_status_e status = proto_deserialize_topology_transaction(buf, &G_context.tx_info);

    if (status != PARSING_OK) {
        ret = status;
        goto exit;
    }

    if (G_context.tx_info.tx_parts_ctx.topology_transaction.has_mapping == false) {
        ret = 0;
        goto exit;
    }

    if (G_context.tx_info.tx_parts_ctx.topology_transaction.operation !=
        com_digitalasset_canton_protocol_v30_Enums_TopologyChangeOp_TOPOLOGY_CHANGE_OP_ADD_REPLACE) {
        ret = SW_TOPOLOGY_UNSUPPORTED_OPERATION;
        goto exit;
    }

    switch (G_context.tx_info.tx_parts_ctx.topology_transaction.mapping.which_mapping) {
        case TOPOLOGY_MAPPING_NAMESPACE_DELEGATION_TAG:
            PRINTF("Processing namespace delegation mapping\n");
            ret = process_namespace_delegation(&G_context.tx_info.tx_parts_ctx.topology_transaction
                                                    .mapping.mapping.namespace_delegation,
                                               &G_context.tx_info);
            break;
        case TOPOLOGY_MAPPING_PARTY_TO_PARTICIPANT_TAG:
            PRINTF("Processing party to participant mapping\n");
            ret = process_party_to_participant(&G_context.tx_info.tx_parts_ctx.topology_transaction
                                                    .mapping.mapping.party_to_participant,
                                               &G_context.tx_info);
            break;
        case TOPOLOGY_MAPPING_PARTY_TO_KEY_MAPPING_TAG:
            PRINTF("Processing party to key mapping\n");
            ret = process_party_to_key_mapping(&G_context.tx_info.tx_parts_ctx.topology_transaction
                                                    .mapping.mapping.party_to_key_mapping,
                                               &G_context.tx_info);
            break;
        default:
            PRINTF("Unknown mapping type in topology transaction: %d\n",
                   G_context.tx_info.tx_parts_ctx.topology_transaction.mapping.which_mapping);
            ret = SW_TOPOLOGY_UNKNOWN_MAPPING_TYPE;
    }

exit:
    release_topology_transaction(&G_context.tx_info);
    return ret;
}
