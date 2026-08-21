#pragma once

#include "cx.h"
#include "com/daml/ledger/api/v2/interactive/interactive_submission_service.pb.h"
#include "bytewriter.h"
#include "tx_types.h"
#include "constants.h"
#include "utils.h"

typedef enum {
    HASH_OK = 0,
    HASH_ERROR_BUFFER_OVERFLOW = 1,
    HASH_ERROR_INVALID_HASH_STRING = 2,
    HASH_ERROR_UNSUPPORTED_VALUE = 3,
    HASH_ERROR_UNKNOWN_NODE_VERSION = 4,
    HASH_ERROR_UNKNOWN_NODE_TYPE = 5,
    HASH_ERROR_FAILED_TO_STORE_NODE_HASH = 6,
    HASH_ERROR_FAILED_TO_LOAD_NODE_HASH = 7,
    HASH_ERROR_MAX_NODE_CHILDREN_EXCEEDED = 8,
} HashError;

typedef struct {
    cx_sha256_t ctx;
} HashWriter;

void hash_transaction(HashWriter *hw, const DamlTransaction *tx);
void finalize_hash_transaction(HashWriter *hw, uint8_t out[SHA256_HASH_LEN]);
void set_node_hash(int node_id, const uint8_t hash[SHA256_HASH_LEN]);

void hash_metadata(HashWriter *hw, const Metadata *md);
void finalize_hash_metadata(HashWriter *hw, uint8_t out[SHA256_HASH_LEN]);

void finalize_hash(const uint8_t tx_hash[SHA256_HASH_LEN],
                   const uint8_t md_hash[SHA256_HASH_LEN],
                   uint8_t out[SHA256_HASH_LEN]);
MUST_CHECK int get_hash_error();

void hw_init(HashWriter *hw);
void hw_put(HashWriter *hw, const void *p, size_t n);
void hw_finalize(HashWriter *hw, uint8_t out[SHA256_HASH_LEN]);
void hw_put_byte(HashWriter *hw, uint8_t b);
void hw_put_u32_be(HashWriter *hw, uint32_t v);
void hw_put_u64_be(HashWriter *hw, uint64_t v);
void encode_bool(HashWriter *hw, bool v);
void encode_int32(HashWriter *hw, int32_t v);
void encode_int64(HashWriter *hw, int64_t v);
void encode_int64_by_ptr(HashWriter *hw, int64_t *v);
void encode_bytes(HashWriter *hw, const uint8_t *data, int32_t len);
void encode_string(HashWriter *hw, const char *s);
void encode_hash(HashWriter *hw, const uint8_t h[SHA256_HASH_LEN]);
void encode_hex_string(HashWriter *hw, const char *hex);
void encode_identifier(HashWriter *hw, const Identifier *id);
void encode_create_start(HashWriter *hw, const Node_CreateCb *c, const uint8_t *seed);
void encode_create_end(HashWriter *hw, const Node_CreateCb *c);
void encode_exercise_start(HashWriter *hw, const Node_ExerciseCb *e, const uint8_t *seed);
void encode_exercise_middle(HashWriter *hw, const Node_ExerciseCb *e);
void encode_exercise_end(HashWriter *hw, const Node_ExerciseCb *e);
void encode_fetch(HashWriter *hw, const Node_Fetch *f);
void encode_rollback(HashWriter *hw, const Node_Rollback *r);
