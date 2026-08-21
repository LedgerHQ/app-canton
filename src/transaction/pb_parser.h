#pragma once

#include "buffer.h"
#include "utils.h"
#include "types.h"

/**
 * Deserialize DamlTransaction protobuf message in structure.
 *
 * @param[in, out] buf
 *   Pointer to buffer with serialized transaction.
 * @param[out]     tx_ctx
 *   Pointer to transaction context structure.
 *
 * @return PARSING_OK if success, error status otherwise.
 *
 */
MUST_CHECK parser_status_e proto_deserialize_daml_tx(buffer_t *buf, transaction_ctx_t *tx_ctx);

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

/**
 * Deserialize Metadata protobuf message in structure.
 *
 * @param[in, out] buf
 *   Pointer to buffer with serialized transaction.
 * @param[out]     tx_ctx
 *   Pointer to transaction context structure.
 *
 * @return PARSING_OK if success, error status otherwise.
 *
 */
MUST_CHECK parser_status_e proto_deserialize_metadata(buffer_t *buf, transaction_ctx_t *tx_ctx);

/**
 * Deserialize TopologyTransaction protobuf message in structure.
 *
 * @param[in, out] buf
 *   Pointer to buffer with serialized transaction.
 * @param[out]     tx_ctx
 *   Pointer to transaction context structure.
 *
 * @return PARSING_OK if success, error status otherwise.
 *
 */
MUST_CHECK parser_status_e proto_deserialize_topology_transaction(buffer_t *buf,
                                                                  transaction_ctx_t *tx_ctx);

/**
 * Release dynamically allocated memory for DamlTransaction protobuf message.
 *
 * @param[in, out]     tx_ctx
 *   Pointer to transaction context structure.
 *
 */
void release_daml_tx(transaction_ctx_t *tx_ctx);

/**
 * Release dynamically allocated memory for DamlTransaction.Node protobuf message.
 *
 * @param[in, out]     tx_ctx
 *   Pointer to transaction context structure.
 *
 */
void release_node(transaction_ctx_t *tx_ctx);

/**
 * Release dynamically allocated memory for Metadata protobuf message.
 *
 * @param[in, out]     tx_ctx
 *   Pointer to transaction context structure.
 *
 */
void release_metadata(transaction_ctx_t *tx_ctx);

/**
 * Release dynamically allocated memory for TopologyTransaction protobuf message.
 *
 * @param[in, out]     tx_ctx
 *   Pointer to transaction context structure.
 *
 */
void release_topology_transaction(transaction_ctx_t *tx_ctx);
