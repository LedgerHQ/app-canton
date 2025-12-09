#include "canonical_hash.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "ledger_assert.h"

#include "pb.h"         // Contains PB_LTYPE macros and basic pb_field_t definitions
#include "pb_common.h"  // Contains pb_field_

// Protocol codegen headers
#include "com/daml/ledger/api/v2/value.pb.h"

#include "constants.h"
#include "mem.h"
#include "utils.h"

/* -------------------------------------------------------------------------- */
/*  Spec constants                                                             */
/* -------------------------------------------------------------------------- */

static const uint8_t PREPARED_TRANSACTION_HASH_PURPOSE[UINT32_T_LEN] = {0x00, 0x00, 0x00, 0x30};
#define HASHING_SCHEME_VERSION ((uint8_t) 2) /* 0x02 */
#define NODE_ENCODING_VERSION  ((uint8_t) 1) /* 0x01 */
#define MAX_ERROR_MSG_LEN      32

/* -------------------------------------------------------------------------- */
/*  Error handling                                                            */
/* -------------------------------------------------------------------------- */

typedef struct {
    uint8_t err_msg[MAX_ERROR_MSG_LEN];
    int err_code;
} HashErrorInfo;

static HashErrorInfo HASH_ERR_INFO = {
    .err_msg = {0},
    .err_code = HASH_OK,
};

static void clear_hash_error() {
    HASH_ERR_INFO.err_code = HASH_OK;
    memset(HASH_ERR_INFO.err_msg, 0, sizeof(HASH_ERR_INFO.err_msg));
}

MUST_CHECK int get_hash_error() {
    if (HASH_ERR_INFO.err_code != HASH_OK) {
        PRINTF("Hash error: '%s', code: %d\n", HASH_ERR_INFO.err_msg, HASH_ERR_INFO.err_code);
    }
    return HASH_ERR_INFO.err_code;
}

static void set_hash_error(HashError err, const char *msg) {
    // Don't overwrite existing error
    if (HASH_ERR_INFO.err_code != HASH_OK) {
        return;
    }

    HASH_ERR_INFO.err_code = err;
    if (msg) {
        strncpy((char *) HASH_ERR_INFO.err_msg, msg, sizeof(HASH_ERR_INFO.err_msg) - 1);
        HASH_ERR_INFO.err_msg[sizeof(HASH_ERR_INFO.err_msg) - 1] = '\0';  // Ensure null termination
    } else {
        HASH_ERR_INFO.err_msg[0] = '\0';  // Clear message if none provided
    }
}

/* -------------------------------------------------------------------------- */
/* Node hash store                                               */
/* -------------------------------------------------------------------------- */

typedef struct {
    int32_t id;
    uint8_t hash[SHA256_HASH_LEN];
} PrecomputedNodeHash;

static PrecomputedNodeHash G_hashed_nodes_store[MAX_NODE_CHILDREN] = {0};
static size_t G_hashed_nodes_store_count = 0;

static void init_node_hash_store() {
    explicit_bzero(G_hashed_nodes_store, sizeof(G_hashed_nodes_store));
    for (size_t i = 0; i < MAX_NODE_CHILDREN; ++i) {
        G_hashed_nodes_store[i].id = -1;  // Mark as empty
    }
}

void set_node_hash(int node_id, const uint8_t hash[SHA256_HASH_LEN]) {
    memcpy(G_hashed_nodes_store[G_hashed_nodes_store_count].hash, hash, SHA256_HASH_LEN);
    G_hashed_nodes_store[G_hashed_nodes_store_count].id = node_id;

    G_hashed_nodes_store_count++;
    G_hashed_nodes_store_count %= MAX_NODE_CHILDREN;

    return;
}

static MUST_CHECK int get_node_hash(const char *node_id, uint8_t out[SHA256_HASH_LEN]) {
    LEDGER_ASSERT(out != NULL, "Null output buffer passed to get_node_hash");

    if (node_id == NULL) {
        return -1;  // No node_id provided
    }

    int node_id_num = atoint(node_id);

    for (size_t i = 0; i < MAX_NODE_CHILDREN; ++i) {
        if (G_hashed_nodes_store[i].id == node_id_num) {
            memcpy(out, G_hashed_nodes_store[i].hash, SHA256_HASH_LEN);
            return 0;
        }
    }

    // Not found
    return -1;
}

/* -------------------------------------------------------------------------- */
/*  HashWriter helper                                                         */
/* -------------------------------------------------------------------------- */

void hw_init(HashWriter *hw) {
    CX_ASSERT(cx_sha256_init_no_throw(&hw->ctx));
}

void hw_put(HashWriter *hw, const void *p, size_t n) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to hw_put");
    LEDGER_ASSERT(p == NULL ? n == 0 : true, "Null pointer with non-zero length passed to hw_put");
    CX_ASSERT(cx_hash_update((cx_hash_t *) &hw->ctx, p, n));
}

void hw_finalize(HashWriter *hw, uint8_t out[SHA256_HASH_LEN]) {
    CX_ASSERT(cx_hash_final((cx_hash_t *) &hw->ctx, out));
}

void hw_put_byte(HashWriter *hw, uint8_t b) {
    hw_put(hw, &b, 1);
}

// Big‑endian helpers
void hw_put_u32_be(HashWriter *hw, uint32_t v) {
    uint8_t t[UINT32_T_LEN] = {(uint8_t) (v >> 24),
                               (uint8_t) (v >> 16),
                               (uint8_t) (v >> 8),
                               (uint8_t) v};
    hw_put(hw, t, UINT32_T_LEN);
}

void hw_put_u64_be(HashWriter *hw, uint64_t v) {
    uint8_t t[UINT64_T_LEN] = {(uint8_t) (v >> 56),
                               (uint8_t) (v >> 48),
                               (uint8_t) (v >> 40),
                               (uint8_t) (v >> 32),
                               (uint8_t) (v >> 24),
                               (uint8_t) (v >> 16),
                               (uint8_t) (v >> 8),
                               (uint8_t) v};
    hw_put(hw, t, UINT64_T_LEN);
}

/* -------------------------------------------------------------------------- */
/*  Encoders                                                                  */
/* -------------------------------------------------------------------------- */

void encode_bool(HashWriter *hw, bool v) {
    hw_put_byte(hw, v ? 1 : 0);
}

void encode_int32(HashWriter *hw, int32_t v) {
    hw_put_u32_be(hw, (uint32_t) v);
}

void encode_int64(HashWriter *hw, int64_t v) {
    hw_put_u64_be(hw, (uint64_t) v);
}

void encode_int64_by_ptr(HashWriter *hw, int64_t *v) {
    hw_put_u64_be(hw, (uint64_t) *v);
}

void encode_bytes(HashWriter *hw, const uint8_t *data, int32_t len) {
    encode_int32(hw, len);
    hw_put(hw, data, (size_t) len);
}

void encode_string(HashWriter *hw, const char *s) {
    encode_bytes(hw, (const uint8_t *) s, (int32_t) strlen(s));
}

void encode_hash(HashWriter *hw, const uint8_t h[SHA256_HASH_LEN]) {
    hw_put(hw, h, SHA256_HASH_LEN);
}

// hex‑decode helper
static uint8_t hex_val(char c) {
    return (uint8_t) ((c >= '0' && c <= '9')   ? c - '0'
                      : (c >= 'a' && c <= 'f') ? 10 + c - 'a'
                                               : 10 + c - 'A');
}

void encode_hex_string(HashWriter *hw, const char *hex) {
    size_t len = strlen(hex);

    if (len % 2 != 0) {
        set_hash_error(HASH_ERROR_INVALID_HASH_STRING, "Hex string must have even length");
        return;
    }

    encode_int32(hw, (int32_t) (len / 2));
    for (size_t i = 0; i < len; i += 2) {
        uint8_t b = (hex_val(hex[i]) << 4) | hex_val(hex[i + 1]);
        hw_put_byte(hw, b);
    }
}

// Generic optional encoder
typedef void (*EncodeFn)(HashWriter *, const void *ctx);
static void encode_optional(HashWriter *hw, bool present, EncodeFn fn, const void *ctx) {
    hw_put_byte(hw, present ? 1 : 0);
    if (present) fn(hw, ctx);
}

// Generic repeated encoder (contiguous array)
static void encode_repeated(HashWriter *hw,
                            size_t count,
                            const void *array,
                            size_t elem_sz,
                            EncodeFn fn) {
    encode_int32(hw, (int32_t) count);
    const uint8_t *p = (const uint8_t *) array;
    for (size_t i = 0; i < count; ++i) fn(hw, p + i * elem_sz);
}

// Helper wrappers invoked by encode_repeated
static void wrap_encode_string(HashWriter *hw, const void *ctx) {
    encode_string(hw, *(char *const *) ctx);
}

static void wrap_encode_identifier(HashWriter *hw, const void *ctx) {
    encode_identifier(hw, (const Identifier *) ctx);
}

static void split_dot_and_encode(HashWriter *hw, const char *dotstr) {
    size_t parts = 1;
    for (const char *p = dotstr; *p; ++p)
        if (*p == '.') ++parts;
    encode_int32(hw, (int32_t) parts);
    const char *start = dotstr;
    while (true) {
        const char *dot = strchr(start, '.');
        size_t len = dot ? (size_t) (dot - start) : strlen(start);
        encode_int32(hw, (int32_t) len);
        hw_put(hw, start, len);
        if (!dot) break;
        start = dot + 1;
    }
}

void encode_identifier(HashWriter *hw, const Identifier *id) {
    encode_string(hw, id->package_id);
    split_dot_and_encode(hw, id->module_name);
    split_dot_and_encode(hw, id->entity_name);
}

static void encode_repeated_node_ids(HashWriter *hw, size_t count, char *const *ids) {
    encode_int32(hw, (int32_t) count);

    for (size_t i = 0; i < count; ++i) {
        uint8_t hash[SHA256_HASH_LEN];
        if (get_node_hash(ids[i], hash) != 0) {
            set_hash_error(HASH_ERROR_FAILED_TO_LOAD_NODE_HASH, "Node id hash not found in store");
            return;
        };
        hw_put(hw, hash, SHA256_HASH_LEN);
    }
}

void encode_create_start(HashWriter *hw, const Node_CreateCb *c, const uint8_t *seed) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to encode_create_start");
    LEDGER_ASSERT(c != NULL, "Null create node passed to encode_create_start");
    // Seed is optional for create nodes, can be NULL

    hw_put_byte(hw, NODE_ENCODING_VERSION);
    encode_string(hw, c->lf_version);
    hw_put_byte(hw, 0x00);
    encode_optional(hw, seed != NULL, (EncodeFn) encode_hash, seed);
    encode_hex_string(hw, c->contract_id);
    encode_string(hw, c->package_name);
    encode_identifier(hw, (const com_daml_ledger_api_v2_Identifier *) &c->template_id);
}

void encode_create_end(HashWriter *hw, const Node_CreateCb *c) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to encode_create_end");
    LEDGER_ASSERT(c != NULL, "Null create node passed to encode_create_end");

    encode_repeated(hw, c->signatories_count, c->signatories, sizeof(char *), wrap_encode_string);
    encode_repeated(hw, c->stakeholders_count, c->stakeholders, sizeof(char *), wrap_encode_string);
}

void encode_exercise_start(HashWriter *hw, const Node_ExerciseCb *e, const uint8_t *seed) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to encode_exercise_start");
    LEDGER_ASSERT(e != NULL, "Null exercise node passed to encode_exercise_start");

    hw_put_byte(hw, NODE_ENCODING_VERSION);
    encode_string(hw, e->lf_version);
    hw_put_byte(hw, 0x01);

    // NOTE: Seed always present for exercise nodes
    LEDGER_ASSERT(seed != NULL, "Missing seed for exercise node");
    encode_hash(hw, seed);
    encode_hex_string(hw, e->contract_id);
    encode_string(hw, e->package_name);
    LEDGER_ASSERT(e->has_template_id, "Missing template_id in exercise node");
    encode_identifier(hw, (Identifier *) &e->template_id);
    encode_repeated(hw, e->signatories_count, e->signatories, sizeof(char *), wrap_encode_string);
    encode_repeated(hw, e->stakeholders_count, e->stakeholders, sizeof(char *), wrap_encode_string);
    encode_repeated(hw,
                    e->acting_parties_count,
                    e->acting_parties,
                    sizeof(char *),
                    wrap_encode_string);
    encode_optional(hw, e->interface_id != NULL, wrap_encode_identifier, e->interface_id);
    encode_string(hw, e->choice_id);
}

void encode_exercise_middle(HashWriter *hw, const Node_ExerciseCb *e) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to encode_exercise_middle");
    LEDGER_ASSERT(e != NULL, "Null exercise node passed to encode_exercise_middle");

    encode_bool(hw, e->consuming);
}

void encode_exercise_end(HashWriter *hw, const Node_ExerciseCb *e) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to encode_exercise_end");
    LEDGER_ASSERT(e != NULL, "Null exercise node passed to encode_exercise_end");

    encode_repeated(hw,
                    e->choice_observers_count,
                    e->choice_observers,
                    sizeof(char *),
                    wrap_encode_string);

    if (e->children_count > MAX_NODE_CHILDREN) {
        set_hash_error(HASH_ERROR_MAX_NODE_CHILDREN_EXCEEDED, "Too many node children");
        return;
    }
    encode_repeated_node_ids(hw, e->children_count, e->children);
}

void encode_fetch(HashWriter *hw, const Node_Fetch *f) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to encode_fetch");
    LEDGER_ASSERT(f != NULL, "Null fetch node passed to encode_fetch");

    hw_put_byte(hw, NODE_ENCODING_VERSION);
    encode_string(hw, f->lf_version);
    hw_put_byte(hw, 0x02);
    encode_hex_string(hw, f->contract_id);
    encode_string(hw, f->package_name);
    encode_identifier(hw, &f->template_id);
    encode_repeated(hw, f->signatories_count, f->signatories, sizeof(char *), wrap_encode_string);
    encode_repeated(hw, f->stakeholders_count, f->stakeholders, sizeof(char *), wrap_encode_string);
    encode_optional(hw, f->interface_id != NULL, wrap_encode_identifier, f->interface_id);
    encode_repeated(hw,
                    f->acting_parties_count,
                    f->acting_parties,
                    sizeof(char *),
                    wrap_encode_string);
}

void encode_rollback(HashWriter *hw, const Node_Rollback *r) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to encode_rollback");
    LEDGER_ASSERT(r != NULL, "Null rollback node passed to encode_rollback");

    hw_put_byte(hw, NODE_ENCODING_VERSION);
    hw_put_byte(hw, 0x03);
    if (r->children_count > MAX_NODE_CHILDREN) {
        set_hash_error(HASH_ERROR_MAX_NODE_CHILDREN_EXCEEDED, "Too many node children");
        return;
    }
    encode_repeated_node_ids(hw, r->children_count, r->children);
}

static void encode_metadata(HashWriter *hw, const Metadata *m) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to encode_metadata");
    LEDGER_ASSERT(m != NULL, "Null metadata passed to encode_metadata");

    hw_put_byte(hw, 0x01);
    encode_repeated(hw,
                    m->submitter_info.act_as_count,
                    m->submitter_info.act_as,
                    sizeof(char *),
                    wrap_encode_string);
    encode_string(hw, m->submitter_info.command_id);
    encode_string(hw, m->transaction_uuid);
    encode_int32(hw, m->mediator_group);
    encode_string(hw, m->synchronizer_id);
    encode_optional(hw,
                    m->has_min_ledger_effective_time,
                    (EncodeFn) encode_int64_by_ptr,
                    &m->min_ledger_effective_time);
    encode_optional(hw,
                    m->has_max_ledger_effective_time,
                    (EncodeFn) encode_int64_by_ptr,
                    &m->max_ledger_effective_time);

    encode_int64(hw, m->preparation_time);
    encode_int32(hw, m->input_contracts_count);
}

void hash_transaction(HashWriter *hw, const DamlTransaction *tx) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to hash_transaction");
    LEDGER_ASSERT(tx != NULL, "Null DamlTransaction passed to hash_transaction");

    // Reset error state
    clear_hash_error();
    init_node_hash_store();

    hw_init(hw);
    hw_put(hw, PREPARED_TRANSACTION_HASH_PURPOSE, 4);

    encode_string(hw, tx->version);
    // Encode nodes count
    encode_int32(hw, (int32_t) tx->roots_count);
}

void finalize_hash_transaction(HashWriter *hw, uint8_t out[SHA256_HASH_LEN]) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to finalize_hash_transaction");
    LEDGER_ASSERT(out != NULL, "Null output buffer passed to finalize_hash_transaction");

    hw_finalize(hw, out);

    PRINTF("TX hash: %.*H\n", 32, out);
}

void hash_metadata(HashWriter *hw, const Metadata *md) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to hash_metadata");
    LEDGER_ASSERT(md != NULL, "Null Metadata passed to hash_metadata");

    hw_init(hw);
    hw_put(hw, PREPARED_TRANSACTION_HASH_PURPOSE, UINT32_T_LEN);
    encode_metadata(hw, md);
}

void finalize_hash_metadata(HashWriter *hw, uint8_t out[SHA256_HASH_LEN]) {
    LEDGER_ASSERT(hw != NULL, "Null HashWriter passed to finalize_hash_metadata");

    hw_finalize(hw, out);

    PRINTF("Metadata hash: %.*H\n", SHA256_HASH_LEN, out);
}

void finalize_hash(const uint8_t tx_hash[SHA256_HASH_LEN],
                   const uint8_t md_hash[SHA256_HASH_LEN],
                   uint8_t out[SHA256_HASH_LEN]) {
    HashWriter hw;

    hw_init(&hw);
    hw_put(&hw, PREPARED_TRANSACTION_HASH_PURPOSE, UINT32_T_LEN);
    hw_put_byte(&hw, HASHING_SCHEME_VERSION);
    hw_put(&hw, tx_hash, SHA256_HASH_LEN);
    hw_put(&hw, md_hash, SHA256_HASH_LEN);

    hw_finalize(&hw, out);
}
