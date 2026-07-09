#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "com/daml/ledger/api/v2/interactive/device.pb.h"
#include "com/digitalasset/canton/version/v1/untyped_versioned_message.pb.h"
#include "com/digitalasset/canton/protocol/v30/topology.pb.h"

typedef com_daml_ledger_api_v2_interactive_DeviceDamlTransaction DamlTransaction;
typedef com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node Node;
typedef com_daml_ledger_api_v2_interactive_DeviceMetadata Metadata;
typedef com_daml_ledger_api_v2_interactive_DeviceMetadata_InputContract InputContract;

/* -------------------------------------------------------------------------- */
/*  Adaption layer for nanopb oneof names                                      */
/* -------------------------------------------------------------------------- */
#define VALUE_ONEOF_FIELD     which_sum
#define VALUE_UNIT_TAG        com_daml_ledger_api_v2_Value_unit_tag
#define VALUE_BOOL_TAG        com_daml_ledger_api_v2_Value_bool__tag
#define VALUE_INT64_TAG       com_daml_ledger_api_v2_Value_int64_tag
#define VALUE_NUMERIC_TAG     com_daml_ledger_api_v2_Value_numeric_tag
#define VALUE_TIMESTAMP_TAG   com_daml_ledger_api_v2_Value_timestamp_tag
#define VALUE_DATE_TAG        com_daml_ledger_api_v2_Value_date_tag
#define VALUE_PARTY_TAG       com_daml_ledger_api_v2_Value_party_tag
#define VALUE_TEXT_TAG        com_daml_ledger_api_v2_Value_text_tag
#define VALUE_CONTRACT_ID_TAG com_daml_ledger_api_v2_Value_contract_id_tag
#define VALUE_OPTIONAL_TAG    com_daml_ledger_api_v2_Value_optional_tag
#define VALUE_LIST_TAG        com_daml_ledger_api_v2_Value_list_tag
#define VALUE_TEXT_MAP_TAG    com_daml_ledger_api_v2_Value_text_map_tag
#define VALUE_RECORD_TAG      com_daml_ledger_api_v2_Value_record_tag
#define VALUE_VARIANT_TAG     com_daml_ledger_api_v2_Value_variant_tag
#define VALUE_ENUM_TAG        com_daml_ledger_api_v2_Value_enum__tag
#define VALUE_GEN_MAP_TAG     com_daml_ledger_api_v2_Value_gen_map_tag

// Node version and kind oneofs
#define NODE_VERSION_ONEOF_FIELD which_versioned_node
#define NODE_V1_TAG com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node_v1_tag
#define NODE_V1_KIND_ONEOF_FIELD which_node_type
#define NODE_V1_CREATE_TAG       com_daml_ledger_api_v2_interactive_transaction_v1_Node_create_tag
#define NODE_V1_EXERCISE_TAG     com_daml_ledger_api_v2_interactive_transaction_v1_Node_exercise_tag
#define NODE_V1_FETCH_TAG        com_daml_ledger_api_v2_interactive_transaction_v1_Node_fetch_tag
#define NODE_V1_ROLLBACK_TAG     com_daml_ledger_api_v2_interactive_transaction_v1_Node_rollback_tag

#define TOPOLOGY_MAPPING_NAMESPACE_DELEGATION_TAG \
    com_digitalasset_canton_protocol_v30_TopologyMapping_namespace_delegation_tag
#define TOPOLOGY_MAPPING_PARTY_TO_PARTICIPANT_TAG \
    com_digitalasset_canton_protocol_v30_TopologyMapping_party_to_participant_tag
#define TOPOLOGY_MAPPING_PARTY_TO_KEY_MAPPING_TAG \
    com_digitalasset_canton_protocol_v30_TopologyMapping_party_to_key_mapping_tag

#define CRYPTO_KEY_FORMAT_RAW \
    com_digitalasset_canton_crypto_v30_CryptoKeyFormat_CRYPTO_KEY_FORMAT_RAW
#define CRYPTO_KEY_FORMAT_DER_X509 \
    com_digitalasset_canton_crypto_v30_CryptoKeyFormat_CRYPTO_KEY_FORMAT_DER_X509_SUBJECT_PUBLIC_KEY_INFO

typedef com_digitalasset_canton_protocol_v30_NamespaceDelegation NamespaceDelegation;
typedef com_digitalasset_canton_protocol_v30_PartyToParticipant PartyToParticipant;
typedef com_digitalasset_canton_protocol_v30_PartyToKeyMapping PartyToKeyMapping;

typedef com_daml_ledger_api_v2_interactive_transaction_v1_Node Node_V1;
typedef com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_NodeSeed NodeSeed;

typedef com_daml_ledger_api_v2_interactive_transaction_v1_Create Node_Create;
typedef com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create Node_CreateCb;
typedef com_daml_ledger_api_v2_interactive_transaction_v1_Exercise Node_Exercise;
typedef com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise Node_ExerciseCb;
typedef com_daml_ledger_api_v2_interactive_transaction_v1_Fetch Node_Fetch;
typedef com_daml_ledger_api_v2_interactive_transaction_v1_Rollback Node_Rollback;

typedef com_daml_ledger_api_v2_cb_Value cbValue;
typedef com_daml_ledger_api_v2_cb_Record cbRecord;
typedef com_daml_ledger_api_v2_cb_RecordField cbRecordField;
typedef com_daml_ledger_api_v2_cb_List cbList;
typedef com_daml_ledger_api_v2_cb_Variant cbVariant;
typedef com_daml_ledger_api_v2_cb_Optional cbOptional;
typedef com_daml_ledger_api_v2_cb_GenMap cbGenMap;
typedef com_daml_ledger_api_v2_cb_GenMap_Entry cbGenMapEntry;
typedef com_daml_ledger_api_v2_cb_TextMap cbTextMap;
typedef com_daml_ledger_api_v2_cb_TextMap_Entry cbTextMapEntry;
typedef com_daml_ledger_api_v2_cb_Enum cbEnum;

typedef com_daml_ledger_api_v2_Value Value;
typedef com_daml_ledger_api_v2_RecordField RecordField;
typedef com_daml_ledger_api_v2_GenMap_Entry GenMapEntry;
typedef com_daml_ledger_api_v2_TextMap_Entry TextMapEntry;
typedef com_daml_ledger_api_v2_Identifier Identifier;

typedef com_digitalasset_canton_version_v1_UntypedVersionedMessage UntypedVersionedMessage;
typedef com_digitalasset_canton_protocol_v30_TopologyTransaction TopologyTransaction;
typedef com_digitalasset_canton_crypto_v30_CryptoKeyFormat CryptoKeyFormat;

#define PARTY_ID_LEN 74  // 3 + 2 + 2*34 + 1 = 74 ldg::hex(fingerprint) + null terminator

typedef enum {
    PARSING_OK = 1,
    NONCE_PARSING_ERROR = -1,
    TO_PARSING_ERROR = -2,
    VALUE_PARSING_ERROR = -3,
    MEMO_LENGTH_ERROR = -4,
    MEMO_PARSING_ERROR = -5,
    MEMO_ENCODING_ERROR = -6,
    WRONG_LENGTH_ERROR = -7
} parser_status_e;

/**
 * Structure for transaction parts context.
 */
typedef struct {
    union {
        DamlTransaction daml_transaction;               /// DAML transaction
        Metadata metadata;                              /// metadata of the transaction
        UntypedVersionedMessage untyped_versioned_msg;  /// untyped versioned message
    };

    union {
        Node node;                                 /// DAML transaction node
        InputContract input_contract;              /// input contract
        TopologyTransaction topology_transaction;  /// topology transaction
    };
} transaction_parts_ctx_t;
