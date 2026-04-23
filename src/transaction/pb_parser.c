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
#include "buffer.h"

#include "pb_parser.h"
#include "types.h"
#include "com/daml/ledger/api/v2/interactive/device.pb.h"
#include "com/digitalasset/canton/version/v1/untyped_versioned_message.pb.h"
#include "com/digitalasset/canton/protocol/v30/topology.pb.h"

#include "pb_decode.h"
#include "ledger_assert.h"

MUST_CHECK parser_status_e proto_deserialize_daml_tx(buffer_t *buf, transaction_ctx_t *tx_ctx) {
    LEDGER_ASSERT(buf != NULL, "Null buffer passed to proto_deserialize_daml_tx");
    LEDGER_ASSERT(tx_ctx != NULL, "Null transaction context passed to proto_deserialize_daml_tx");

    pb_istream_t stream = pb_istream_from_buffer(buf->ptr, buf->size);

    PRINTF("Decoding Daml transaction from buffer of size %d bytes\n", buf->size);

    if (!pb_decode(&stream,
                   com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_fields,
                   &tx_ctx->tx_parts_ctx.daml_transaction)) {
        PRINTF("Failed to decode Daml transaction: %s\n", PB_GET_ERROR(&stream));
        return VALUE_PARSING_ERROR;
    }

    return PARSING_OK;
}

MUST_CHECK parser_status_e proto_deserialize_metadata(buffer_t *buf, transaction_ctx_t *tx_ctx) {
    LEDGER_ASSERT(buf != NULL, "Null buffer passed to proto_deserialize_metadata");
    LEDGER_ASSERT(tx_ctx != NULL, "Null transaction context passed to proto_deserialize_metadata");

    pb_istream_t stream = pb_istream_from_buffer(buf->ptr, buf->size);

    PRINTF("Decoding Metadata from buffer of size %d bytes\n", buf->size);

    if (!pb_decode(&stream,
                   com_daml_ledger_api_v2_interactive_DeviceMetadata_fields,
                   &tx_ctx->tx_parts_ctx.metadata)) {
        PRINTF("Failed to decode Metadata: %s\n", PB_GET_ERROR(&stream));
        return VALUE_PARSING_ERROR;
    }

    return PARSING_OK;
}

MUST_CHECK parser_status_e proto_deserialize_topology_transaction(buffer_t *buf,
                                                                  transaction_ctx_t *tx_ctx) {
    LEDGER_ASSERT(buf != NULL, "Null buffer passed to proto_deserialize_topology_transaction");
    LEDGER_ASSERT(tx_ctx != NULL,
                  "Null transaction context passed to proto_deserialize_topology_transaction");

    pb_istream_t stream = pb_istream_from_buffer(buf->ptr, buf->size);

    PRINTF("Decoding Topology Transaction from buffer of size %d bytes\n", buf->size);

    if (!pb_decode(&stream,
                   com_digitalasset_canton_version_v1_UntypedVersionedMessage_fields,
                   &tx_ctx->tx_parts_ctx.untyped_versioned_msg)) {
        PRINTF("Failed to decode Untyped Versioned Message: %s\n", PB_GET_ERROR(&stream));
        return VALUE_PARSING_ERROR;
    }

    PRINTF("Decoded Untyped Versioned Message.\n");

    if (tx_ctx->tx_parts_ctx.untyped_versioned_msg.which_wrapper !=
            com_digitalasset_canton_version_v1_UntypedVersionedMessage_data_tag ||
        tx_ctx->tx_parts_ctx.untyped_versioned_msg.data == NULL ||
        tx_ctx->tx_parts_ctx.untyped_versioned_msg.data->size == 0) {
        PRINTF("Missing topology wrapper data\n");
        return VALUE_PARSING_ERROR;
    }

    PRINTF("Untyped versioned message transaction wrapper type: %d\n",
           tx_ctx->tx_parts_ctx.untyped_versioned_msg.which_wrapper);
    PRINTF("Topology Transaction data size: %d\n",
           tx_ctx->tx_parts_ctx.untyped_versioned_msg.data->size);
    PRINTF("Topology Transaction wrapper bytes: %.*H\n",
           tx_ctx->tx_parts_ctx.untyped_versioned_msg.data->size,
           tx_ctx->tx_parts_ctx.untyped_versioned_msg.data->bytes);

    pb_istream_t stream2 =
        pb_istream_from_buffer(tx_ctx->tx_parts_ctx.untyped_versioned_msg.data->bytes,
                               tx_ctx->tx_parts_ctx.untyped_versioned_msg.data->size);

    if (!pb_decode(&stream2,
                   com_digitalasset_canton_protocol_v30_TopologyTransaction_fields,
                   &tx_ctx->tx_parts_ctx.topology_transaction)) {
        PRINTF("Failed to decode Topology Transaction: %s\n", PB_GET_ERROR(&stream2));
        return VALUE_PARSING_ERROR;
    }

    PRINTF("Decoded Topology Transaction.\n");

    return PARSING_OK;
}

void release_daml_tx(transaction_ctx_t *tx_ctx) {
    LEDGER_ASSERT(tx_ctx != NULL, "Null transaction context passed to release_daml_tx");

    pb_release(com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_fields,
               &tx_ctx->tx_parts_ctx.daml_transaction);
}

void release_metadata(transaction_ctx_t *tx_ctx) {
    LEDGER_ASSERT(tx_ctx != NULL, "Null transaction context passed to release_metadata");

    pb_release(com_daml_ledger_api_v2_interactive_DeviceMetadata_fields,
               &tx_ctx->tx_parts_ctx.metadata);
}

void release_topology_transaction(transaction_ctx_t *tx_ctx) {
    LEDGER_ASSERT(tx_ctx != NULL,
                  "Null transaction context passed to release_topology_transaction");

    pb_release(com_digitalasset_canton_version_v1_UntypedVersionedMessage_fields,
               &tx_ctx->tx_parts_ctx.untyped_versioned_msg);
    pb_release(com_digitalasset_canton_protocol_v30_TopologyTransaction_fields,
               &tx_ctx->tx_parts_ctx.topology_transaction);
}
