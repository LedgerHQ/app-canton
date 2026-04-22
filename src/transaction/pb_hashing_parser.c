#include "pb_hashing_parser.h"

#include "buffer.h"
#include "canonical_hash.h"
#include "types.h"
#include "com/daml/ledger/api/v2/interactive/device.pb.h"

#include "pb_decode.h"
#include "ledger_assert.h"
#include "constants.h"

#define VALUE_ELEM_COUNT_NONE -1

typedef struct {
    transaction_ctx_t *tx_info;
    int32_t node_id;
    bool is_root_node;
    int32_t value_elem_count;
    HashWriter node_hw;
} cb_parser_ctx_t;

static cb_parser_ctx_t ctx;

MUST_CHECK static bool decode_value_variant(pb_istream_t *stream,
                                            const pb_field_t *field,
                                            void **arg);

/* -------------------------------------------------------------------------- */
/* Callbacks for counting number of elements                                  */
/* -------------------------------------------------------------------------- */

MUST_CHECK static bool count_identifier(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to count_identifier");

    com_daml_ledger_api_v2_cb_Identifier id = com_daml_ledger_api_v2_cb_Identifier_init_zero;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Identifier_fields, &id)) {
        PRINTF("Failed to decode Identifier: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_cb_Identifier_fields, &id);

    ctx.value_elem_count++;

    return true;
}

MUST_CHECK static bool count_record_field(pb_istream_t *stream,
                                          const pb_field_t *field,
                                          void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to count_record_field");

    cbRecordField rf = com_daml_ledger_api_v2_cb_RecordField_init_zero;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_RecordField_fields, &rf)) {
        PRINTF("Failed to decode Record field: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_cb_RecordField_fields, &rf);

    ctx.value_elem_count++;

    return true;
}

MUST_CHECK static bool count_list_elem(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to count_list_elem");

    cbValue v = com_daml_ledger_api_v2_cb_Value_init_zero;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Value_fields, &v)) {
        PRINTF("Failed to decode List: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_cb_Value_fields, &v);

    ctx.value_elem_count++;

    return true;
}

MUST_CHECK static bool count_text_map_entry(pb_istream_t *stream,
                                            const pb_field_t *field,
                                            void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to count_text_map_entry");

    cbTextMapEntry e = com_daml_ledger_api_v2_cb_TextMap_Entry_init_zero;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_TextMap_Entry_fields, &e)) {
        PRINTF("Failed to decode TextMap entry: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_cb_TextMap_Entry_fields, &e);

    ctx.value_elem_count++;

    return true;
}

MUST_CHECK static bool count_gen_map_entry(pb_istream_t *stream,
                                           const pb_field_t *field,
                                           void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to count_gen_map_entry");

    cbGenMapEntry e = com_daml_ledger_api_v2_cb_GenMap_Entry_init_zero;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_GenMap_Entry_fields, &e)) {
        PRINTF("Failed to decode GenMap entry: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_cb_GenMap_Entry_fields, &e);

    ctx.value_elem_count++;

    return true;
}

MUST_CHECK static bool count_value(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) stream;
    (void) arg;
    LEDGER_ASSERT(field != NULL, "NULL field passed to count_value");

    ctx.value_elem_count = 0;

    switch (field->tag) {
        case com_daml_ledger_api_v2_cb_Value_unit_tag:
        case com_daml_ledger_api_v2_cb_Value_bool__tag:
        case com_daml_ledger_api_v2_cb_Value_int64_tag:
        case com_daml_ledger_api_v2_cb_Value_numeric_tag:
        case com_daml_ledger_api_v2_cb_Value_timestamp_tag:
        case com_daml_ledger_api_v2_cb_Value_date_tag:
        case com_daml_ledger_api_v2_cb_Value_party_tag:
        case com_daml_ledger_api_v2_cb_Value_text_tag:
        case com_daml_ledger_api_v2_cb_Value_contract_id_tag:
            break;
        case com_daml_ledger_api_v2_cb_Value_optional_tag: {
            cbOptional *msg = field->pData;
            msg->value.funcs.decode = &count_list_elem;
        } break;
        case com_daml_ledger_api_v2_cb_Value_list_tag: {
            cbList *msg = field->pData;
            msg->elements.funcs.decode = &count_list_elem;
        } break;
        case com_daml_ledger_api_v2_cb_Value_text_map_tag: {
            cbTextMap *msg = field->pData;
            msg->entries.funcs.decode = &count_text_map_entry;
        } break;
        case com_daml_ledger_api_v2_cb_Value_gen_map_tag: {
            cbGenMap *msg = field->pData;
            msg->entries.funcs.decode = &count_gen_map_entry;
        } break;
        case com_daml_ledger_api_v2_cb_Value_record_tag: {
            cbRecord *msg = field->pData;
            msg->fields.funcs.decode = &count_record_field;
        } break;
        case com_daml_ledger_api_v2_cb_Value_variant_tag: {
            cbVariant *msg = field->pData;
            msg->variant_id.funcs.decode = &count_identifier;
        } break;
        case com_daml_ledger_api_v2_cb_Value_enum__tag:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown Value type %d", field->tag);
    }

    return true;
}

MUST_CHECK static bool count_value_helper(pb_istream_t *stream) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to count_value_helper");

    cbValue c = com_daml_ledger_api_v2_cb_Value_init_zero;
    pb_istream_t saved_stream = *stream;

    c.cb_sum.funcs.decode = &count_value;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Value_fields, &c)) {
        PRINTF("Failed to count Input contract argument: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_cb_Value_fields, &c);

    // Restore stream state
    *stream = saved_stream;

    return true;
}

MUST_CHECK static bool count_gen_map_helper(pb_istream_t *stream) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to count_gen_map_helper");

    cbGenMapEntry e = com_daml_ledger_api_v2_cb_GenMap_Entry_init_zero;
    pb_istream_t saved_stream = *stream;

    e.value.cb_sum.funcs.decode = &count_value;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_GenMap_Entry_fields, &e)) {
        PRINTF("Failed to count GenMap entry: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_cb_GenMap_Entry_fields, &e);

    // Restore stream state
    *stream = saved_stream;

    return true;
}

MUST_CHECK static bool count_text_map_helper(pb_istream_t *stream) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to count_text_map_helper");

    cbTextMapEntry e = com_daml_ledger_api_v2_cb_TextMap_Entry_init_zero;
    pb_istream_t saved_stream = *stream;

    e.value.cb_sum.funcs.decode = &count_value;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_TextMap_Entry_fields, &e)) {
        PRINTF("Failed to count TextMap entry: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_cb_TextMap_Entry_fields, &e);

    // Restore stream state
    *stream = saved_stream;

    return true;
}

MUST_CHECK static bool count_record_field_helper(pb_istream_t *stream) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to count_record_field_helper");

    cbRecordField rf = com_daml_ledger_api_v2_cb_RecordField_init_zero;
    rf.value.cb_sum.funcs.decode = &count_value;

    pb_istream_t saved_stream = *stream;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_RecordField_fields, &rf)) {
        PRINTF("Failed to decode Record field for counting: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_cb_RecordField_fields, &rf);

    *stream = saved_stream;  // restore stream position
    return true;
}

/* -------------------------------------------------------------------------- */
/*  Callbacks to decode Value types                                           */
/* -------------------------------------------------------------------------- */

static void decode_value_primitive_variants(cbValue *v) {
    LEDGER_ASSERT(v != NULL, "NULL cbValue pointer passed to decode_value_primitive_variants");

    switch (v->which_sum) {
        case com_daml_ledger_api_v2_cb_Value_unit_tag: {
            PRINTF("Decoding unit\n");
            hw_put_byte(&ctx.node_hw, 0x00);
        } break;
        case com_daml_ledger_api_v2_cb_Value_bool__tag: {
            PRINTF("Decoding bool: %s\n", v->bool_ ? "true" : "false");
            hw_put_byte(&ctx.node_hw, 0x01);
            encode_bool(&ctx.node_hw, v->bool_);
        } break;
        case com_daml_ledger_api_v2_cb_Value_int64_tag: {
            PRINTF("Decoding int64: %lld\n", v->int64);
            hw_put_byte(&ctx.node_hw, 0x02);
            encode_int64(&ctx.node_hw, v->int64);
        } break;
        case com_daml_ledger_api_v2_cb_Value_date_tag: {
            PRINTF("Decoding date: %lld\n", v->date);
            hw_put_byte(&ctx.node_hw, 0x05);
            encode_int32(&ctx.node_hw, v->date);
        } break;
        case com_daml_ledger_api_v2_cb_Value_timestamp_tag: {
            PRINTF("Decoding timestamp: %lld\n", v->timestamp);
            hw_put_byte(&ctx.node_hw, 0x04);
            encode_int64(&ctx.node_hw, v->timestamp);
        } break;
        case com_daml_ledger_api_v2_cb_Value_numeric_tag: {
            PRINTF("Decoding numeric: %s\n", v->numeric);
            hw_put_byte(&ctx.node_hw, 0x03);
            encode_string(&ctx.node_hw, v->numeric);
        } break;
        case com_daml_ledger_api_v2_cb_Value_party_tag: {
            PRINTF("Decoding party: %s\n", v->party);
            hw_put_byte(&ctx.node_hw, 0x06);
            encode_string(&ctx.node_hw, v->party);
        } break;
        case com_daml_ledger_api_v2_cb_Value_text_tag: {
            PRINTF("Decoding text: %s\n", v->text);
            hw_put_byte(&ctx.node_hw, 0x07);
            encode_string(&ctx.node_hw, v->text);
        } break;
        case com_daml_ledger_api_v2_cb_Value_contract_id_tag: {
            PRINTF("Decoding contract_id: %s\n", v->contract_id);
            hw_put_byte(&ctx.node_hw, 0x08);
            encode_hex_string(&ctx.node_hw, v->contract_id);
        } break;
        default: {
            // Not a primitive variant, nothing to do here.
        } break;
    }
}

MUST_CHECK static bool decode_record_field_label(pb_istream_t *stream,
                                                 const pb_field_t *field,
                                                 void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_record_field_label");

    // Read string from stream
    char label_buffer[DEFAULT_DECODE_BUFFER_SIZE] = {0};
    size_t len =
        stream->bytes_left < sizeof(label_buffer) ? stream->bytes_left : sizeof(label_buffer) - 1;

    if (!pb_read(stream, (pb_byte_t *) label_buffer, len)) {
        PRINTF("Failed to read string from stream\n");
        return false;
    }

    PRINTF("Decoded Record field label: %s\n", label_buffer);

    // Encode the label
    hw_put_byte(&ctx.node_hw, 0x01);  // encode optional field
    encode_string(&ctx.node_hw, label_buffer);

    return true;
}

MUST_CHECK static bool decode_variant_constructor(pb_istream_t *stream,
                                                  const pb_field_t *field,
                                                  void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_variant_constructor");

    // Read string from stream
    char constructor_buffer[DEFAULT_DECODE_BUFFER_SIZE] = {0};
    size_t len = stream->bytes_left < sizeof(constructor_buffer) ? stream->bytes_left
                                                                 : sizeof(constructor_buffer) - 1;

    if (!pb_read(stream, (pb_byte_t *) constructor_buffer, len)) {
        PRINTF("Failed to read string from stream\n");
        return false;
    }

    PRINTF("Decoded constructor str: %s\n", constructor_buffer);

    encode_string(&ctx.node_hw, constructor_buffer);

    return true;
}

MUST_CHECK static bool decode_textmap_key(pb_istream_t *stream,
                                          const pb_field_t *field,
                                          void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_textmap_key");

    // Read string from stream
    char key_buffer[DEFAULT_DECODE_BUFFER_SIZE] = {0};
    size_t len =
        stream->bytes_left < sizeof(key_buffer) ? stream->bytes_left : sizeof(key_buffer) - 1;

    if (!pb_read(stream, (pb_byte_t *) key_buffer, len)) {
        PRINTF("Failed to read string from stream\n");
        return false;
    }

    PRINTF("Decoded TextMap key: %s\n", key_buffer);

    encode_string(&ctx.node_hw, key_buffer);

    return true;
}

MUST_CHECK static bool decode_node_id_field(pb_istream_t *stream,
                                            const pb_field_t *field,
                                            void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_node_id_field");

    PRINTF("Decoding node_id field\n");

    // Read string from stream
    char node_id_str[4] = {0};

    size_t len = stream->bytes_left;
    if (len >= sizeof(node_id_str)) {
        PRINTF("Node id too long\n");
        return false;
    }

    if (!pb_read(stream, (pb_byte_t *) node_id_str, len)) {
        PRINTF("Failed to decode node_id from stream\n");
        return false;
    }

    DamlTransaction *daml_tx = &ctx.tx_info->tx_parts_ctx.daml_transaction;
    for (size_t i = 0; i < daml_tx->roots_count; ++i) {
        if (daml_tx->roots[i] != NULL && strcmp(daml_tx->roots[i], node_id_str) == 0) {
            ctx.is_root_node = true;
            break;
        }
    }

    ctx.node_id = atoint(node_id_str);

    return true;
}

MUST_CHECK static bool decode_record_field(pb_istream_t *stream,
                                           const pb_field_t *field,
                                           void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_record_field");

    PRINTF("Decoding Record fields\n");

    if (!count_record_field_helper(stream)) {
        return false;
    }

    cbRecordField rf = com_daml_ledger_api_v2_cb_RecordField_init_zero;
    rf.value.cb_sum.funcs.decode = &decode_value_variant;
    rf.label.funcs.decode = &decode_record_field_label;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_RecordField_fields, &rf)) {
        PRINTF("Failed to decode Record field: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    decode_value_primitive_variants(&rf.value);

    pb_release(com_daml_ledger_api_v2_cb_RecordField_fields, &rf);

    return true;
}

MUST_CHECK static bool decode_list_elem(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_list_elem");

    PRINTF("Decoding List elements\n");
    if (!count_value_helper(stream)) {
        return false;
    }

    cbValue v = com_daml_ledger_api_v2_cb_Value_init_zero;
    v.cb_sum.funcs.decode = &decode_value_variant;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Value_fields, &v)) {
        PRINTF("Failed to decode List: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    decode_value_primitive_variants(&v);

    PRINTF("/Decoding List elements\n");
    pb_release(com_daml_ledger_api_v2_cb_Value_fields, &v);

    return true;
}

MUST_CHECK static bool decode_value(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_value");

    PRINTF("Decoding Optional value\n");
    if (!count_value_helper(stream)) {
        return false;
    }

    cbValue v = com_daml_ledger_api_v2_cb_Value_init_zero;
    v.cb_sum.funcs.decode = &decode_value_variant;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Value_fields, &v)) {
        PRINTF("Failed to decode Optional value: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    decode_value_primitive_variants(&v);

    PRINTF("/Decoding Optional value\n");

    pb_release(com_daml_ledger_api_v2_cb_Value_fields, &v);

    return true;
}

MUST_CHECK static bool decode_text_map_entry(pb_istream_t *stream,
                                             const pb_field_t *field,
                                             void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_text_map_entry");

    PRINTF("Decoding TextMap entry\n");

    if (!count_text_map_helper(stream)) {
        return false;
    }

    cbTextMapEntry entry = com_daml_ledger_api_v2_cb_TextMap_Entry_init_zero;

    entry.key.funcs.decode = &decode_textmap_key;
    entry.value.cb_sum.funcs.decode = &decode_value_variant;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_TextMap_Entry_fields, &entry)) {
        PRINTF("Failed to decode TextMap entry: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    decode_value_primitive_variants(&entry.value);

    PRINTF("/Decoding TextMap entry\n");
    pb_release(com_daml_ledger_api_v2_cb_TextMap_Entry_fields, &entry);

    return true;
}

MUST_CHECK static bool decode_gen_map_entry(pb_istream_t *stream,
                                            const pb_field_t *field,
                                            void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_gen_map_entry");

    PRINTF("Decoding GenMap entry\n");
    if (!count_gen_map_helper(stream)) {
        return false;
    }

    pb_istream_t saved_stream = *stream;

    cbGenMapEntry entry = com_daml_ledger_api_v2_cb_GenMap_Entry_init_zero;
    entry.key.cb_sum.funcs.decode = &decode_value_variant;
    entry.value.cb_sum.funcs.decode = NULL;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_GenMap_Entry_fields, &entry)) {
        PRINTF("Failed to decode GenMap entry: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    decode_value_primitive_variants(&entry.key);
    pb_release(com_daml_ledger_api_v2_cb_GenMap_Entry_fields, &entry);

    *stream = saved_stream;  // restore stream position
    entry.key.cb_sum.funcs.decode = NULL;
    entry.value.cb_sum.funcs.decode = &decode_value_variant;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_GenMap_Entry_fields, &entry)) {
        PRINTF("Failed to decode GenMap entry: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    decode_value_primitive_variants(&entry.value);
    pb_release(com_daml_ledger_api_v2_cb_GenMap_Entry_fields, &entry);

    PRINTF("/Decoding GenMap entry\n");

    return true;
}

MUST_CHECK static bool decode_enum(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_enum");

    PRINTF("Decoding Enum\n");
    cbEnum e = com_daml_ledger_api_v2_cb_Enum_init_zero;
    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Enum_fields, &e)) {
        PRINTF("Failed to decode Enum: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    // Encode optional field presence
    if (e.has_enum_id) {
        hw_put_byte(&ctx.node_hw, 0x01);
        encode_identifier(&ctx.node_hw, (const com_daml_ledger_api_v2_Identifier *) &e.enum_id);
    } else {
        hw_put_byte(&ctx.node_hw, 0x00);
    }

    encode_string(&ctx.node_hw, e.constructor);

    pb_release(com_daml_ledger_api_v2_cb_Enum_fields, &e);

    PRINTF("/Decoding Enum\n");

    return true;
}

MUST_CHECK static bool decode_identifier(pb_istream_t *stream,
                                         const pb_field_t *field,
                                         void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_identifier");

    PRINTF("Decoding Identifier\n");

    com_daml_ledger_api_v2_cb_Identifier id = com_daml_ledger_api_v2_cb_Identifier_init_zero;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Identifier_fields, &id)) {
        PRINTF("Failed to decode Identifier: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    encode_identifier(&ctx.node_hw, (const com_daml_ledger_api_v2_Identifier *) &id);

    PRINTF("/Decoding Identifier\n");

    pb_release(com_daml_ledger_api_v2_cb_Identifier_fields, &id);

    return true;
}

MUST_CHECK static bool decode_record_id(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_record_id");

    PRINTF("Decoding Record ID Identifier\n");
    hw_put_byte(&ctx.node_hw, 0x01);  // encode optional field presence
    bool res = decode_identifier(stream, field, arg);

    if (ctx.value_elem_count != VALUE_ELEM_COUNT_NONE) {
        encode_int32(&ctx.node_hw, ctx.value_elem_count);
        ctx.value_elem_count = VALUE_ELEM_COUNT_NONE;
    }

    return res;
}

MUST_CHECK static bool decode_value_variant(pb_istream_t *stream,
                                            const pb_field_t *field,
                                            void **arg) {
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_value_variant");
    LEDGER_ASSERT(field != NULL, "NULL field passed to decode_value_variant");

    PRINTF("Decoding value variant %d\n", field->tag);

    switch (field->tag) {
        case com_daml_ledger_api_v2_cb_Value_unit_tag:
        case com_daml_ledger_api_v2_cb_Value_bool__tag:
        case com_daml_ledger_api_v2_cb_Value_int64_tag:
        case com_daml_ledger_api_v2_cb_Value_numeric_tag:
        case com_daml_ledger_api_v2_cb_Value_timestamp_tag:
        case com_daml_ledger_api_v2_cb_Value_date_tag:
        case com_daml_ledger_api_v2_cb_Value_party_tag:
        case com_daml_ledger_api_v2_cb_Value_text_tag:
        case com_daml_ledger_api_v2_cb_Value_contract_id_tag: {
        } break;
        case com_daml_ledger_api_v2_cb_Value_optional_tag: {
            cbOptional *msg = field->pData;
            msg->value.funcs.decode = &decode_value;
            hw_put_byte(&ctx.node_hw, 0x09);
            // Encode optional field presence
            hw_put_byte(&ctx.node_hw, ctx.value_elem_count == 0 ? 0x00 : 0x01);
            ctx.value_elem_count = VALUE_ELEM_COUNT_NONE;
        } break;
        case com_daml_ledger_api_v2_cb_Value_list_tag: {
            cbList *msg = field->pData;
            msg->elements.funcs.decode = &decode_list_elem;
            hw_put_byte(&ctx.node_hw, 0x0A);
            encode_int32(&ctx.node_hw, ctx.value_elem_count);
            ctx.value_elem_count = VALUE_ELEM_COUNT_NONE;
        } break;
        case com_daml_ledger_api_v2_cb_Value_text_map_tag: {
            cbTextMap *msg = field->pData;
            msg->entries.funcs.decode = &decode_text_map_entry;
            hw_put_byte(&ctx.node_hw, 0x0B);
            encode_int32(&ctx.node_hw, ctx.value_elem_count);
            ctx.value_elem_count = VALUE_ELEM_COUNT_NONE;
        } break;
        case com_daml_ledger_api_v2_cb_Value_gen_map_tag: {
            cbGenMap *msg = field->pData;
            msg->entries.funcs.decode = &decode_gen_map_entry;
            hw_put_byte(&ctx.node_hw, 0x0F);
            encode_int32(&ctx.node_hw, ctx.value_elem_count);
            ctx.value_elem_count = VALUE_ELEM_COUNT_NONE;
        } break;
        case com_daml_ledger_api_v2_cb_Value_record_tag: {
            cbRecord *msg = field->pData;
            msg->record_id.funcs.decode = &decode_record_id;
            msg->fields.funcs.decode = &decode_record_field;
            hw_put_byte(&ctx.node_hw, 0x0C);
        } break;
        case com_daml_ledger_api_v2_cb_Value_variant_tag: {
            cbVariant *msg = field->pData;
            msg->variant_id.funcs.decode = &decode_identifier;
            msg->constructor.funcs.decode = &decode_variant_constructor;
            msg->value.funcs.decode = &decode_value;
            hw_put_byte(&ctx.node_hw, 0x0D);
            // Encode optional field presence
            hw_put_byte(&ctx.node_hw, ctx.value_elem_count == 0 ? 0x00 : 0x01);
            ctx.value_elem_count = VALUE_ELEM_COUNT_NONE;
        } break;
        case com_daml_ledger_api_v2_cb_Value_enum__tag: {
            hw_put_byte(&ctx.node_hw, 0x0E);
            return decode_enum(stream, field, arg);
        } break;
        default:
            LEDGER_ASSERT(false, "Unknown Value type %d", field->tag);
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  Callbacks to decode Transaction nodes                                      */
/* -------------------------------------------------------------------------- */

MUST_CHECK static bool decode_value_field(pb_istream_t *stream,
                                          const pb_field_t *field,
                                          void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_value_field");

    PRINTF("Decoding Value field\n");
    if (!count_value_helper(stream)) {
        return false;
    }

    cbValue v = com_daml_ledger_api_v2_cb_Value_init_zero;

    v.cb_sum.funcs.decode = &decode_value_variant;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Value_fields, &v)) {
        PRINTF("Failed to decode Value field: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    decode_value_primitive_variants(&v);

    pb_release(com_daml_ledger_api_v2_cb_Value_fields, &v);

    return true;
}

MUST_CHECK static bool decode_create(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_create");

    PRINTF("Decode Create node\n");

    // Save stream state to rewind later
    pb_istream_t saved_stream = *stream;

    com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create c_cb =
        com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_init_zero;

    // Decoding Create node's plain fields
    if (!pb_decode(stream,
                   com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields,
                   &c_cb)) {
        PRINTF("Failed to decode Create node: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    // Hashing fields up to `argument` field
    const uint8_t *seed = NULL;
    DamlTransaction *daml_tx = &ctx.tx_info->tx_parts_ctx.daml_transaction;
    if (ctx.node_id >= 0) {
        if (daml_tx->node_seeds_count > 0) {
            for (size_t i = 0; i < daml_tx->node_seeds_count; ++i) {
                if (daml_tx->node_seeds[i].node_id == ctx.node_id) {
                    PRINTF("Found seed for node id %d\n", ctx.node_id);
                    if (daml_tx->node_seeds[i].seed == NULL ||
                        daml_tx->node_seeds[i].seed->size != SHA256_HASH_LEN) {
                        PRINTF("Invalid node seed length\n");
                        pb_release(
                            com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields,
                            &c_cb);
                        return false;
                    }
                    seed = daml_tx->node_seeds[i].seed->bytes;
                    break;
                }
            }
        }
    }

    encode_create_start(&ctx.node_hw, &c_cb, seed);

    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields, &c_cb);

    // Rewind stream to the beginning of Create node CB message
    *stream = saved_stream;

    // Decoding Create node CB recursive field `argument` and hashing it inside callbacks
    c_cb.argument.funcs.decode = &decode_value_field;
    if (!pb_decode(stream,
                   com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields,
                   &c_cb)) {
        PRINTF("Failed to decode Create node (CB): %s\n", PB_GET_ERROR(stream));
        return false;
    }

    // Finishing hashing Create node
    encode_create_end(&ctx.node_hw, &c_cb);

    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields, &c_cb);

    PRINTF("/Decode Create node\n");

    return true;
}

MUST_CHECK static bool decode_exercise(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_exercise");

    PRINTF("Decode Exercise node\n");

    com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise e =
        com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise_init_zero;

    pb_istream_t stream_prev = *stream;

    // 1. Decode plain fields and calculate hash till `chosen_value`
    if (!pb_decode(stream,
                   com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise_fields,
                   &e)) {
        PRINTF("Failed to decode Exercise node: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    const uint8_t *seed = NULL;
    DamlTransaction *daml_tx = &ctx.tx_info->tx_parts_ctx.daml_transaction;
    if (ctx.node_id >= 0) {
        for (size_t i = 0; i < daml_tx->node_seeds_count; ++i) {
            if (daml_tx->node_seeds[i].node_id == ctx.node_id) {
                PRINTF("Found seed for node id %d\n", ctx.node_id);
                if (daml_tx->node_seeds[i].seed == NULL ||
                    daml_tx->node_seeds[i].seed->size != SHA256_HASH_LEN) {
                    PRINTF("Invalid node seed length\n");
                    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise_fields,
                               &e);
                    return false;
                }
                seed = daml_tx->node_seeds[i].seed->bytes;
                break;
            }
        }
    }

    if (seed == NULL) {
        PRINTF("Missing seed for node id %d\n", ctx.node_id);
        pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise_fields, &e);
        return false;
    }

    encode_exercise_start(&ctx.node_hw, &e, seed);

    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise_fields, &e);
    *stream = stream_prev;  // rewind stream

    // 2. Decode and hash `chosen_value` field and fields up to `exercise_result` field
    e.chosen_value.funcs.decode = &decode_value_field;
    e.exercise_result.funcs.decode = NULL;
    if (!pb_decode(stream,
                   com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise_fields,
                   &e)) {
        PRINTF("Failed to decode Exercise node: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    encode_exercise_middle(&ctx.node_hw, &e);

    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise_fields, &e);
    *stream = stream_prev;  // rewind stream

    // 3. Decode and hash `exercise_result` field and rest of the fields
    // Encode optional `exercise_result` field presence
    hw_put_byte(&ctx.node_hw, 0x01);
    e.chosen_value.funcs.decode = NULL;
    e.exercise_result.funcs.decode = &decode_value_field;
    if (!pb_decode(stream,
                   com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise_fields,
                   &e)) {
        PRINTF("Failed to decode Exercise node: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    encode_exercise_end(&ctx.node_hw, &e);

    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Exercise_fields, &e);

    return true;
}

MUST_CHECK static bool decode_fetch(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_fetch");

    PRINTF("Decode Fetch node\n");

    com_daml_ledger_api_v2_interactive_transaction_v1_Fetch f =
        com_daml_ledger_api_v2_interactive_transaction_v1_Fetch_init_zero;

    if (!pb_decode(stream, com_daml_ledger_api_v2_interactive_transaction_v1_Fetch_fields, &f)) {
        PRINTF("Failed to decode Fetch node: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    encode_fetch(&ctx.node_hw, &f);

    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_Fetch_fields, &f);

    return true;
}

MUST_CHECK static bool decode_rollback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_rollback");

    PRINTF("Decode Rollback node\n");

    com_daml_ledger_api_v2_interactive_transaction_v1_Rollback r =
        com_daml_ledger_api_v2_interactive_transaction_v1_Rollback_init_zero;

    if (!pb_decode(stream, com_daml_ledger_api_v2_interactive_transaction_v1_Rollback_fields, &r)) {
        PRINTF("Failed to decode Rollback node: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    encode_rollback(&ctx.node_hw, &r);

    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_Rollback_fields, &r);

    return true;
}

MUST_CHECK static bool node_decode_callback(pb_istream_t *stream,
                                            const pb_field_t *field,
                                            void **arg) {
    (void) arg;
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to node_decode_callback");
    LEDGER_ASSERT(field != NULL, "NULL field passed to node_decode_callback");

    switch (field->tag) {
        case NODE_V1_EXERCISE_TAG: {
            PRINTF("Decoding Exercise node\n");
            return decode_exercise(stream, field, arg);
        } break;
        case NODE_V1_CREATE_TAG: {
            PRINTF("Decoding Create node\n");
            return decode_create(stream, field, arg);
        } break;
        case NODE_V1_FETCH_TAG: {
            PRINTF("Decoding Fetch node\n");
            return decode_fetch(stream, field, arg);
        } break;
        case NODE_V1_ROLLBACK_TAG: {
            PRINTF("Decoding Rollback node\n");
            return decode_rollback(stream, field, arg);
        } break;
        default:
            LEDGER_ASSERT(false, "Unsupported node type %d", field->tag);
    }

    return true;
}

MUST_CHECK static bool versioned_node_decode_callback(pb_istream_t *stream,
                                                      const pb_field_t *field,
                                                      void **arg) {
    (void) stream;
    (void) arg;
    LEDGER_ASSERT(field != NULL, "NULL field passed to versioned_node_decode_callback");

    com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node *node = field->message;

    if (node->NODE_VERSION_ONEOF_FIELD == NODE_V1_TAG) {
        node->v1.cb_node_type.funcs.decode = &node_decode_callback;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  Entry points for parsing Nodes/Input contracts                            */
/* -------------------------------------------------------------------------- */

parser_status_e proto_deserialize_node(buffer_t *buf, transaction_ctx_t *tx_ctx) {
    LEDGER_ASSERT(buf != NULL, "NULL buffer passed to proto_deserialize_node");
    LEDGER_ASSERT(tx_ctx != NULL, "NULL transaction context passed to proto_deserialize_node");

    pb_istream_t stream = pb_istream_from_buffer(buf->ptr, buf->size);

    PRINTF("Decoding Node from buffer of size %d bytes\n", buf->size);

    // Init parsing ctx
    ctx.tx_info = tx_ctx;
    ctx.node_id = -1;
    ctx.is_root_node = false;
    ctx.value_elem_count = VALUE_ELEM_COUNT_NONE;

    hw_init(&ctx.node_hw);

    tx_ctx->tx_parts_ctx.node.node_id.funcs.decode = decode_node_id_field;
    tx_ctx->tx_parts_ctx.node.cb_versioned_node.funcs.decode = &versioned_node_decode_callback;

    if (!pb_decode(&stream,
                   com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node_fields,
                   &tx_ctx->tx_parts_ctx.node)) {
        PRINTF("Decode failed: %s\n", PB_GET_ERROR(&stream));
        return VALUE_PARSING_ERROR;
    }

    uint8_t node_hash[SHA256_HASH_LEN];
    hw_finalize(&ctx.node_hw, node_hash);

    PRINTF("Node id %d hash: %.*H\n", ctx.node_id, SHA256_HASH_LEN, node_hash);

    if (ctx.is_root_node) {
        encode_hash(&tx_ctx->hasher, node_hash);
    } else {
        set_node_hash(ctx.node_id, node_hash);
    }

    pb_release(com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node_fields,
               &tx_ctx->tx_parts_ctx.node);

    return PARSING_OK;
}

parser_status_e proto_deserialize_input_contract(buffer_t *buf, transaction_ctx_t *tx_ctx) {
    LEDGER_ASSERT(buf != NULL, "NULL buffer passed to proto_deserialize_input_contract");
    LEDGER_ASSERT(tx_ctx != NULL,
                  "NULL transaction context passed to proto_deserialize_input_contract");

    pb_istream_t stream = pb_istream_from_buffer(buf->ptr, buf->size);

    PRINTF("Decoding Input contract from buffer of size %d bytes\n", buf->size);

    // Init parsing ctx
    ctx.tx_info = tx_ctx;
    ctx.node_id = -1;
    ctx.is_root_node = false;
    ctx.value_elem_count = VALUE_ELEM_COUNT_NONE;

    hw_init(&ctx.node_hw);

    tx_ctx->tx_parts_ctx.input_contract.cb_contract.funcs.decode = &decode_create;

    if (!pb_decode(&stream,
                   com_daml_ledger_api_v2_interactive_DeviceMetadata_InputContract_fields,
                   &tx_ctx->tx_parts_ctx.input_contract)) {
        PRINTF("Failed to decode Input contract: %s\n", PB_GET_ERROR(&stream));
        return VALUE_PARSING_ERROR;
    }

    uint8_t node_hash[SHA256_HASH_LEN];
    hw_finalize(&ctx.node_hw, node_hash);

    encode_int64(&tx_ctx->hasher, tx_ctx->tx_parts_ctx.input_contract.created_at);
    PRINTF("Contract hash: %.*H\n", SHA256_HASH_LEN, node_hash);
    encode_hash(&tx_ctx->hasher, node_hash);

    pb_release(com_daml_ledger_api_v2_interactive_DeviceMetadata_InputContract_fields,
               &tx_ctx->tx_parts_ctx.input_contract);

    return PARSING_OK;
}
