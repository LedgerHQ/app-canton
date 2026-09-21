#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <stdint.h>   // uint*_t
#include <string.h>   // memmove, strcmp, strlen

#include "cx.h"  // cx_hash_sha256
#include "ledger_assert.h"
#include "os.h"  // PRINTF, PIC
#include "pb_decode.h"

#include "com/daml/ledger/api/v2/interactive/device.pb.h"
#include "constants.h"
#include "pb_holding_parser.h"
#include "node_values_check.h"
#include "pb_node_display_definitions.h"  // identifier_config_t
#include "tx_types.h"
#include "utils.h"

/* --- Which templates hold value, and where their fields are --- */
// A create's argument is a tree of records whose fields carry labels, and protobuf attaches no
// meaning to them, so each path is a dotted list of labels to ctx down. "amount.initialAmount"
// means descend into the field labelled amount, then take initialAmount.
typedef struct {
    const char *module_name;  // "Splice.Amulet"         -- is this create a holding?
    const char *entity_name;  // "Amulet"
    const char *owner_path;   // "owner"                 -- and where inside it are the fields?
    const char *amount_path;  // "amount.initialAmount"
    const char *admin_path;   // "dso"
} holding_path_config_t;

// Read out of the recorded transactions in tests/tx_examples, not guessed. Paths vary per template,
// which is the point: LockedAmulet wraps its Amulet, and the registry Holding has no dso field.
static const holding_path_config_t HOLDING_TEMPLATES[] = {
    {"Splice.Amulet", "Amulet", "owner", "amount.initialAmount", "dso"},
    {"Splice.Amulet", "LockedAmulet", "amulet.owner", "amulet.amount.initialAmount", "amulet.dso"},
    // instrument.source carries the registrar that issued this holding. Inferred from the CBTC and
    // SBC fixtures, not read from Splice's Daml source.
    {"Utility.Registry.Holding.V0.Holding", "Holding", "owner", "amount", "instrument.source"},
    // A transfer instruction is a pending offer nobody holds yet, so the account it is destined for
    // counts as its owner. Also an inference.
    {"Splice.AmuletTransferInstruction",
     "AmuletTransferInstruction",
     "transfer.receiver",
     "transfer.amount",
     "transfer.instrumentId.admin"},
    {"Splice.ExternalPartyAmuletRules", "TransferCommand", "receiver", "amount", "dso"}};

// Known, and holding no value for an account. Reward coupons and activity markers pay a validator
// or an app the screen never names, and the pre-approval proposal moves nothing. Listed so that an
// unlisted template really is unknown.
static const identifier_config_t NON_HOLDING_TEMPLATES[] = {
    {"Splice.Amulet", "AppRewardCoupon"},
    {"Splice.Amulet", "ValidatorRewardCoupon"},
    {"Splice.Amulet", "FeaturedAppActivityMarker"},
    {"Utility.Registry.V0.Holding.Transfer", "ExecutedTransfer"},
    {"Splice.Wallet.TransferPreapproval", "TransferPreapprovalProposal"}};

// Sized the way pb_node_display_config.c sizes DISPLAY_CONFIGS_NB, kept file-local because the
// tables and their only reader live here.
static const size_t HOLDING_TEMPLATES_NB = sizeof(HOLDING_TEMPLATES) / sizeof(HOLDING_TEMPLATES[0]);
static const size_t NON_HOLDING_TEMPLATES_NB =
    sizeof(NON_HOLDING_TEMPLATES) / sizeof(NON_HOLDING_TEMPLATES[0]);

/* --- Parser state --- */
#define MAX_LABEL_PATH_LEN 64  // "amulet.amount.initialAmount" is the longest configured path
#define MAX_LABEL_DEPTH    6   // real holdings nest 3 deep, and Nano X is stack-sensitive here

// Each value is hashed the moment it is captured rather than kept as a string. Party identifiers
// run to about 115 bytes, and this state lives in .bss, which on this device is taken out of the
// stack the deepest input-contract parse needs.
static struct {
    const holding_path_config_t *cfg;  // NULL when this create holds no value
    char path[MAX_LABEL_PATH_LEN];
    uint8_t depth;
    uint8_t owner[SHA256_HASH_LEN];
    uint8_t amount[SHA256_HASH_LEN];
    uint8_t admin[SHA256_HASH_LEN];
    bool has_owner, has_amount, has_admin;
} ctx;

MUST_CHECK static bool decode_value(pb_istream_t *stream, const pb_field_t *field, void **arg);

/* --- Template lookup --- */
// The payload is attacker-controlled, and protobuf calls these Required without enforcing it, so a
// missing name must read as "no match" rather than reach strcmp with NULL.
MUST_CHECK static bool names_match(const com_daml_ledger_api_v2_cb_Identifier *id,
                                   const char *module_name,
                                   const char *entity_name) {
    if (id == NULL || id->module_name == NULL || id->entity_name == NULL) {
        return false;
    }
    return strcmp(id->module_name, (const char *) PIC(module_name)) == 0 &&
           strcmp(id->entity_name, (const char *) PIC(entity_name)) == 0;
}

MUST_CHECK static const holding_path_config_t *find_holding(
    const com_daml_ledger_api_v2_cb_Identifier *id) {
    for (size_t i = 0; i < HOLDING_TEMPLATES_NB; i++) {
        const holding_path_config_t *cfg =
            (const holding_path_config_t *) PIC(&HOLDING_TEMPLATES[i]);
        if (names_match(id, cfg->module_name, cfg->entity_name)) {
            return cfg;
        }
    }
    return NULL;
}

MUST_CHECK static bool is_non_holding(const com_daml_ledger_api_v2_cb_Identifier *id) {
    for (size_t i = 0; i < NON_HOLDING_TEMPLATES_NB; i++) {
        const identifier_config_t *cfg =
            (const identifier_config_t *) PIC(&NON_HOLDING_TEMPLATES[i]);
        if (names_match(id, cfg->module_name, cfg->entity_name)) {
            return true;
        }
    }
    return false;
}

/* --- Label path tracking --- */
// A label is only pushed when the field carries one and it fits, so nothing here may assume a
// level was added. decode_record_field is the one that undoes a push, and it does so by cutting
// the path back to the length it had before the field, which is right in every case.
static void path_push(const char *label) {
    size_t len = strlen(ctx.path);
    size_t add = strlen(label);
    if (len + (len > 0 ? 1 : 0) + add >= MAX_LABEL_PATH_LEN) {
        // This path cannot be written down, so it can never legitimately equal a configured one.
        // Stop matching for the rest of this create rather than read the fields under it at a
        // depth they do not have. The create then reports an unreadable holding.
        ctx.cfg = NULL;
        return;
    }
    if (len > 0) {
        ctx.path[len++] = '.';
    }
    memmove(ctx.path + len, label, add + 1);
}

MUST_CHECK static bool path_is(const char *configured) {
    return strcmp(ctx.path, (const char *) PIC(configured)) == 0;
}

// Hash one found value into its slot. Nothing compares these as text, so the digest is all
// that has to survive until the create closes.
static void set_field_value(uint8_t slot[SHA256_HASH_LEN], const char *text) {
    cx_hash_sha256((const uint8_t *) text, strlen(text), slot, SHA256_HASH_LEN);
}

// Look up the current path among the configured ones and set the value if it matches, the way
// find_tx_field does for display fields. The path is the only thing that says which party owns
// this contract and which number is its amount.
static void find_holding_field(const cbValue *value) {
    const char *text = NULL;
    if (ctx.cfg == NULL) {
        return;
    }
    if (value->which_sum == com_daml_ledger_api_v2_cb_Value_party_tag) {
        text = value->party;
    } else if (value->which_sum == com_daml_ledger_api_v2_cb_Value_numeric_tag) {
        text = value->numeric;
    }
    if (text == NULL) {
        return;
    }

    // Set the first occurrence of each; a repeat would belong to another contract.
    if (!ctx.has_owner && path_is(ctx.cfg->owner_path)) {
        set_field_value(ctx.owner, text);
        ctx.has_owner = true;
    } else if (!ctx.has_amount && path_is(ctx.cfg->amount_path)) {
        set_field_value(ctx.amount, text);
        ctx.has_amount = true;
    } else if (!ctx.has_admin && path_is(ctx.cfg->admin_path)) {
        set_field_value(ctx.admin, text);
        ctx.has_admin = true;
    }
}

/* --- Record descent --- */
MUST_CHECK static bool decode_label(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    char label[DEFAULT_DECODE_BUFFER_SIZE] = {0};
    size_t len = stream->bytes_left < sizeof(label) ? stream->bytes_left : sizeof(label) - 1;
    (void) field, (void) arg;
    if (!pb_read(stream, (pb_byte_t *) label, len)) {
        return false;
    }
    path_push(label);
    return true;
}

MUST_CHECK static bool decode_record_field(pb_istream_t *stream,
                                           const pb_field_t *field,
                                           void **arg) {
    cbRecordField rf = com_daml_ledger_api_v2_cb_RecordField_init_zero;
    // Where this field's siblings start. Cutting back to it undoes whatever decode_label pushed,
    // including nothing at all when the field has no label or the label did not fit.
    size_t parent_len = strlen(ctx.path);

    (void) field, (void) arg;
    rf.label.funcs.decode = &decode_label;
    rf.value.cb_sum.funcs.decode = &decode_value;
    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_RecordField_fields, &rf)) {
        // pb_decode frees nothing of what it already built, and this field is a local, so the
        // strings under it are only reachable from here.
        pb_release(com_daml_ledger_api_v2_cb_RecordField_fields, &rf);
        return false;
    }
    find_holding_field(&rf.value);
    ctx.path[parent_len] = '\0';
    pb_release(com_daml_ledger_api_v2_cb_RecordField_fields, &rf);
    return true;
}

// Only records are descended. No configured path runs through a list, a map, an optional or a
// variant, so everything else is a leaf this ctx has no reason to open.
MUST_CHECK static bool decode_value(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) arg;
    if (field->tag != com_daml_ledger_api_v2_cb_Value_record_tag || ctx.depth >= MAX_LABEL_DEPTH) {
        return true;
    }

    cbRecord *record = field->pData;
    record->fields.funcs.decode = &decode_record_field;
    ctx.depth++;
    bool ok = pb_decode(stream, com_daml_ledger_api_v2_cb_Record_fields, record);
    ctx.depth--;
    // Released on both paths. The record belongs to the parent tree, but the parent is a local of
    // decode_argument, and a failure unwinds past it without releasing anything.
    pb_release(com_daml_ledger_api_v2_cb_Record_fields, record);
    return ok;
}

MUST_CHECK static bool decode_argument(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    cbValue value = com_daml_ledger_api_v2_cb_Value_init_zero;

    (void) field, (void) arg;
    value.cb_sum.funcs.decode = &decode_value;
    bool ok = pb_decode(stream, com_daml_ledger_api_v2_cb_Value_fields, &value);
    // Released on both paths, for the same reason as the record above.
    pb_release(com_daml_ledger_api_v2_cb_Value_fields, &value);
    return ok;
}

/* --- Node kinds --- */
// Two passes over the same create: the first reads the template so the paths are known, the second
// parses the argument with them. The stream is rewound in between, the same trick the hashing
// parser uses to hash a create's plain fields before its argument.
MUST_CHECK static bool decode_create(pb_istream_t *stream) {
    com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create probe =
        com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_init_zero;
    com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create create =
        com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_init_zero;
    pb_istream_t saved_stream = *stream;
    const holding_path_config_t *cfg = NULL;
    bool classified = false;
    bool ok = false;
    if (!pb_decode(stream,
                   com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields,
                   &probe)) {
        // The probe holds the contract id, the package name, the template id and both party
        // arrays, and it is a local, so nothing else can give them back.
        pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields, &probe);
        return false;
    }
    if (probe.has_template_id) {
        cfg = find_holding(&probe.template_id);
        classified = cfg != NULL || is_non_holding(&probe.template_id);
    }
    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields, &probe);
    if (cfg == NULL) {
        if (!classified) {
            values_report_unknown_template();
        }
        // Leave the stream where the probe left it. Rewinding without a second pass to consume the
        // submessage would hand nanopb an unread one, and it would then materialise the whole plain
        // Create, argument tree included, which exhausts the heap.
        return true;
    }

    *stream = saved_stream;
    ctx.cfg = cfg;
    ctx.depth = 0;
    ctx.path[0] = '\0';
    ctx.has_owner = false;
    ctx.has_amount = false;
    ctx.has_admin = false;
    create.argument.funcs.decode = &decode_argument;
    ok = pb_decode(stream,
                   com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields,
                   &create);
    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields, &create);
    if (ok) {
        if (ctx.has_owner && ctx.has_amount && ctx.has_admin) {
            values_report_holding(ctx.owner, ctx.amount, ctx.admin);
        } else {
            // The template is listed but a path did not resolve, so the table no longer describes
            // this contract and what it holds is unknown.
            values_report_unreadable_holding();
        }
    }

    ctx.cfg = NULL;
    return ok;
}

// Only a create writes value. Everything else has to be read past all the same: leaving bytes
// unconsumed would hand nanopb an unread submessage, and it would then materialise the whole plain
// node, argument tree included, which is what neither the heap nor the stack can take.
MUST_CHECK static bool decode_node_type(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) arg;
    if (field->tag == NODE_V1_CREATE_TAG) {
        return decode_create(stream);
    }
    return pb_read(stream, NULL, stream->bytes_left);
}

MUST_CHECK static bool decode_versioned_node(pb_istream_t *stream,
                                             const pb_field_t *field,
                                             void **arg) {
    (void) stream, (void) arg;

    com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node *node = field->message;
    if (node->NODE_VERSION_ONEOF_FIELD == NODE_V1_TAG) {
        node->v1.cb_node_type.funcs.decode = &decode_node_type;
    }
    return true;
}

void values_collect_from_node(const buffer_t *buf) {
    com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node node =
        com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node_init_zero;

    LEDGER_ASSERT(buf != NULL, "NULL buf in values_collect_from_node");
    if (!values_still_collecting()) {
        return;  // clear signing is already off, so there is nothing left to learn
    }

    node.cb_versioned_node.funcs.decode = &decode_versioned_node;

    pb_istream_t stream = pb_istream_from_buffer(buf->ptr, buf->size);
    if (!pb_decode(&stream,
                   com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node_fields,
                   &node)) {
        values_report_parse_failure();
    }
    pb_release(com_daml_ledger_api_v2_interactive_DeviceDamlTransaction_Node_fields, &node);
}
