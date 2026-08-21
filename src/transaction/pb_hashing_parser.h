#pragma once

#include "buffer.h"
#include "utils.h"
#include "types.h"

/**
 * Deserialize Metadata.InputContract protobuf message in structure.
 *
 * @param[in, out] buf
 *   Pointer to buffer with serialized transaction.
 * @param[out]     tx_ctx
 *   Pointer to transaction context structure.
 *
 * @return PARSING_OK if success, error status otherwise.
 *
 */
MUST_CHECK parser_status_e proto_deserialize_input_contract(buffer_t *buf,
                                                            transaction_ctx_t *tx_ctx);

/**
 * Deserialize DamlTransaction.Node protobuf message in structure.
 *
 * @param[in, out] buf
 *   Pointer to buffer with serialized transaction.
 * @param[out]     tx_ctx
 *   Pointer to transaction context structure.
 *
 * @return PARSING_OK if success, error status otherwise.
 *
 */
MUST_CHECK parser_status_e proto_deserialize_node(buffer_t *buf, transaction_ctx_t *tx_ctx);
