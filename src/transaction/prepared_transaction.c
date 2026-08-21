#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <string.h>   // memset, explicit_bzero

#include "os.h"
#include "cx.h"
#include "buffer.h"
#include "mem.h"  // for app_mem_alloc

#include "sign_tx.h"
#include "sw.h"
#include "globals.h"
#include "display.h"
#include "tx_types.h"
#include "pb_parser.h"
#include "prepared_transaction.h"
#include "validate.h"
#include "canonical_hash.h"
#include "pb_decode.h"
#include "pb_node_display_parser.h"
#include "pb_hashing_parser.h"

typedef enum {
    RECEIVING_DAML_TX_PART,              /// Receiving part of DAML transaction
    RECEIVING_DAML_NODES,                /// Receiving DAML nodes
    RECEIVING_METADATA,                  /// Receiving metadata
    RECEIVING_METADATA_INPUT_CONTRACTS,  /// Receiving input contracts
} prepared_tx_receiving_state_e;

static prepared_tx_receiving_state_e tx_state = RECEIVING_DAML_TX_PART;
static int process_prepared_tx_finalize();

void process_prepared_tx_init() {
    tx_state = RECEIVING_DAML_TX_PART;
}

MUST_CHECK int process_prepared_tx_part(buffer_t *buf) {
    LEDGER_ASSERT(buf != NULL, "Null buffer passed to process_prepared_tx_part");

    switch (tx_state) {
        case RECEIVING_DAML_TX_PART: {
            parser_status_e status = proto_deserialize_daml_tx(buf, &G_context.tx_info);

            if (status != PARSING_OK) {
                PRINTF("Failed to parse DAML transaction part: %d\n", status);
                return SW_TX_PARSING_FAIL;
            }

            // No need to check for hashing errors here, set_hash_error is not called in
            // this function. Critical errors are handled with assertions (CX_ASSERT,
            // LEDGER_ASSERT).
            hash_transaction(&G_context.tx_info.hasher,
                             &G_context.tx_info.tx_parts_ctx.daml_transaction);

            G_context.tx_info.recv_node_idx = 0;
            tx_state = G_context.tx_info.tx_parts_ctx.daml_transaction.nodes_count == 0
                           ? RECEIVING_METADATA
                           : RECEIVING_DAML_NODES;
        } break;

        case RECEIVING_DAML_NODES: {
            // Hash calculated inside callback during deserialization
            parser_status_e status = proto_deserialize_node(buf, &G_context.tx_info);

            if (status != PARSING_OK) {
                PRINTF("Failed to parse DAML Node part: %d\n", status);
                return SW_TX_PARSING_FAIL;
            }

            // A hashing error might have occurred during deserialization :
            // We hash fields on the fly inside callbacks for DAML nodes.
            int res = get_hash_error();
            if (res != HASH_OK) {
                PRINTF("Failed to hash DAML Node. Hash error code : %d\n", res);
                release_daml_tx(&G_context.tx_info);
                return SW_TX_HASH_FAIL;
            }

            // Parse node for clear signing availability
            res = parse_node_for_display(buf);
            if (res != 0) {
                PRINTF("Failed to parse DAML Node for display: %d\n", res);
                release_daml_tx(&G_context.tx_info);
                return SW_TX_PARSING_FAIL;
            }

            G_context.tx_info.recv_node_idx++;

            if (G_context.tx_info.recv_node_idx ==
                G_context.tx_info.tx_parts_ctx.daml_transaction.nodes_count) {
                tx_state = RECEIVING_METADATA;
            }
        } break;
        case RECEIVING_METADATA: {
            // No need to check for hashing errors here, set_hash_error is not called in
            // this function. Critical errors are handled with assertions (CX_ASSERT,
            // LEDGER_ASSERT).
            finalize_hash_transaction(&G_context.tx_info.hasher, G_context.tx_info.partial_tx_hash);
            release_daml_tx(&G_context.tx_info);

            parser_status_e status = proto_deserialize_metadata(buf, &G_context.tx_info);

            if (status != PARSING_OK) {
                PRINTF("Failed to parse Metadata part: %d\n", status);
                release_metadata(&G_context.tx_info);
                return SW_TX_PARSING_FAIL;
            }

            // No need to check for hashing errors here, set_hash_error is not called in
            // this function. Critical errors are handled with assertions (CX_ASSERT,
            // LEDGER_ASSERT).
            hash_metadata(&G_context.tx_info.hasher, &G_context.tx_info.tx_parts_ctx.metadata);

            release_metadata(&G_context.tx_info);

            G_context.tx_info.recv_node_idx = 0;

            if (G_context.tx_info.tx_parts_ctx.metadata.input_contracts_count == 0) {
                return process_prepared_tx_finalize();
            } else {
                tx_state = RECEIVING_METADATA_INPUT_CONTRACTS;
            }

        } break;
        case RECEIVING_METADATA_INPUT_CONTRACTS: {
            // Hash calculated inside callback during deserialization
            parser_status_e status = proto_deserialize_input_contract(buf, &G_context.tx_info);

            if (status != PARSING_OK) {
                PRINTF("Failed to parse Input Contract part: %d\n", status);
                return SW_TX_PARSING_FAIL;
            }

            // A hashing error might have occurred during deserialization :
            // We hash fields on the fly inside callbacks for input contracts.
            int res = get_hash_error();
            if (res != HASH_OK) {
                PRINTF("Failed to hash metadata. Hash error code : %d\n", res);
                return SW_TX_HASH_FAIL;
            }

            res = parse_input_contract_for_display(buf);
            if (res != 0) {
                PRINTF("Failed to parse Input Contract for display: %d\n", res);
                return SW_TX_PARSING_FAIL;
            }

            G_context.tx_info.recv_node_idx++;

            if (G_context.tx_info.recv_node_idx ==
                G_context.tx_info.tx_parts_ctx.metadata.input_contracts_count) {
                return process_prepared_tx_finalize();
            }
        } break;
        default:
            PRINTF("Invalid state during processing prepared tx part: %d\n", tx_state);
            return SW_BAD_STATE;
    }

    return 0;
}

static MUST_CHECK int process_prepared_tx_finalize() {
    if (G_context.state != STATE_PARSED) {
        PRINTF("Invalid state: expected STATE_PARSED, got %d\n", G_context.state);
        return SW_BAD_STATE;
    }

    // Finalize metadata hash (critical errors handled with assertions)
    finalize_hash_metadata(&G_context.tx_info.hasher, G_context.tx_info.partial_md_hash);

    // Finalize hash (critical errors handled with assertions)
    finalize_hash(G_context.tx_info.partial_tx_hash,
                  G_context.tx_info.partial_md_hash,
                  G_context.tx_info.m_hash);

    G_context.tx_info.m_hash_len = 32;

    return 0;
}
