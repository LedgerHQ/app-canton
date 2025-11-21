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
#include "utils.h"  // for atoull

#include <stdio.h>
#include <time.h>
#include <stddef.h>
#include "bytewriter.h"
#include "read.h"

/* -------------------------------------------------------------------------- */
/* Constants                                                                  */
/* -------------------------------------------------------------------------- */

#define MAX_DISPLAY_FIELDS_NB             10
#define TOKEN_TRANSFER_FIELDS_NB          5
#define TOKEN_TRANSFER_ACCEPT_FIELDS_NB   6
#define TOKEN_TRANSFER_REJECT_FIELDS_NB   6
#define TOKEN_TRANSFER_WITHDRAW_FIELDS_NB 3

#define NATIVE_TRANSFER_FIELDS_NB      4
#define PREAPPROVAL_PROPOSAL_FIELDS_NB 3
#define PROXY_TRANSFER_FIELDS_NB       5

#define MAX_FIELD_PATH_LEN  128
#define MAX_HASH_TABLE_SIZE 53  // Large enough to avoid collisions for small sets (faster lookups)

#define TOKEN_TRANSFER_INSTRUMENT_ID_FIELD_INDEX 3
#define PREAPPROVAL_ASSET_FIELD_INDEX            1

#define NATIVE_COIN_TICKER        "CC"
#define NATIVE_COIN_INSTRUMENT_ID "Amulet"

/* -------------------------------------------------------------------------- */
/* Function pointers type                                                     */
/* -------------------------------------------------------------------------- */

typedef struct pb_callback_context_t pb_callback_context_t;
typedef struct tx_field_t tx_field_t;
typedef void (*field_format_callback_t)(pb_callback_context_t *ctx, tx_field_t *field);

/* -------------------------------------------------------------------------- */
/* Structures                                                                 */
/* -------------------------------------------------------------------------- */
typedef struct {
    const char *module_name;
    const char *entity_name;
} identifier_config_t;

typedef struct {
    const char *path;
    const char *item_name;
    field_format_callback_t format_callback;
    bool mandatory;
} field_config_t;

struct tx_field_t {
    char *value;                   // Dynamically allocated field value
    size_t value_len;              // Length of the field
    const field_config_t *config;  // Pointer to const config
    bool found;                    // Mutable state
    bool display;                  // Whether the field should be displayed
};

typedef struct {
    const identifier_config_t *identifier;
    const identifier_config_t *metadata_contract_identifier;
    const field_config_t *const *fields;
    size_t fields_count;
    const char *review_title;
    const char *review_finish;
} display_config_t;

struct pb_callback_context_t {
    char *field_path;
    transaction_ctx_t *tx_info;
    tx_field_t *tx_fields;
    uint8_t nb_fields;
    const char *review_title;
    const char *review_finish;
};

/* -------------------------------------------------------------------------- */
/* Static functions declarations                                              */
/* -------------------------------------------------------------------------- */

static bool decode_value_variant(pb_istream_t *stream, const pb_field_t *field, void **arg);
static void format_amount_field(pb_callback_context_t *ctx, tx_field_t *field);
static void format_token_amount_field(pb_callback_context_t *ctx, tx_field_t *field);
static void format_native_amount_field(pb_callback_context_t *ctx, tx_field_t *field);
static void format_timestamp_field(pb_callback_context_t *ctx, tx_field_t *field);

/* -------------------------------------------------------------------------- */
/* Review titles for different transaction types                              */
/* -------------------------------------------------------------------------- */

#define TOKEN_TRANSFER_REVIEW_TITLE           "Review transaction to send tokens"
#define TOKEN_TRANSFER_REVIEW_FINISH          "Sign transaction to send tokens?"
#define TOKEN_TRANSFER_ACCEPT_REVIEW_TITLE    "Review transaction to accept incoming transfer"
#define TOKEN_TRANSFER_ACCEPT_REVIEW_FINISH   "Sign transaction to accept incoming transfer?"
#define TOKEN_TRANSFER_REJECT_REVIEW_TITLE    "Review transaction to reject incoming transfer"
#define TOKEN_TRANSFER_REJECT_REVIEW_FINISH   "Sign transaction to reject incoming transfer?"
#define TOKEN_TRANSFER_WITHDRAW_REVIEW_TITLE  "Review transaction to withdraw transfer offer"
#define TOKEN_TRANSFER_WITHDRAW_REVIEW_FINISH "Sign transaction to withdraw transfer offer?"
#define NATIVE_COIN_TRANSFER_REVIEW_TITLE     "Review transaction to send Canton Coin"
#define NATIVE_COIN_TRANSFER_REVIEW_FINISH    "Sign transaction to send Canton Coin?"
#define PREAPPROVAL_PROPOSAL_REVIEW_TITLE     "Review transaction to pre-approve incoming transfers"
#define PREAPPROVAL_PROPOSAL_REVIEW_FINISH    "Sign transaction to pre-approve incoming transfers?"
#define PREAPPROVAL_ASSET_FIELD_VALUE         "Canton Coin (CC)"

/* -------------------------------------------------------------------------- */
/* Known identifiers for matching transaction types                           */
/* -------------------------------------------------------------------------- */

static const identifier_config_t PREAPPROVAL_PROPOSAL_ID = {
    .module_name = "Splice.Wallet.TransferPreapproval",
    .entity_name = "TransferPreapprovalProposal"};

static const identifier_config_t TOKEN_TRANSFER_ID = {
    .module_name = "Splice.Api.Token.TransferInstructionV1",
    .entity_name = "TransferFactory_Transfer"};

static const identifier_config_t NATIVE_COIN_TRANSFER_ID = {
    .module_name = "Splice.ExternalPartyAmuletRules",
    .entity_name = "ExternalPartyAmuletRules_CreateTransferCommand"};

static const identifier_config_t TOKEN_TRANSFER_ACCEPT_ID = {
    .module_name = "Splice.Api.Token.TransferInstructionV1",
    .entity_name = "TransferInstruction_Accept"};

static const identifier_config_t TOKEN_TRANSFER_INSTRUCTION_METADATA_ID = {
    .module_name = "Splice.AmuletTransferInstruction",
    .entity_name = "AmuletTransferInstruction"};

static const identifier_config_t TOKEN_TRANSFER_REJECT_ID = {
    .module_name = "Splice.Api.Token.TransferInstructionV1",
    .entity_name = "TransferInstruction_Reject"};

static const identifier_config_t TOKEN_TRANSFER_WITHDRAW_ID = {
    .module_name = "Splice.Api.Token.TransferInstructionV1",
    .entity_name = "TransferInstruction_Withdraw"};

static const identifier_config_t TOKEN_TRANSFER_PROXY_ID = {
    .module_name = "Splice.Util.FeaturedApp.WalletUserProxy",
    .entity_name = "WalletUserProxy_TransferFactory_Transfer"};

/* -------------------------------------------------------------------------- */
/* Known instrument id to ticker mapping                                      */
/* -------------------------------------------------------------------------- */

static const char *INSTRUMENT_ID_TO_TICKER_MAPPING[] = {
    "Amulet",
    NATIVE_COIN_TICKER,  // Canton Coin
    "amulet",
    NATIVE_COIN_TICKER,  // Canton Coin lowercase
};

/* -------------------------------------------------------------------------- */
/* Field display configuration                                                */
/* -------------------------------------------------------------------------- */

// Transfer commands fields
const field_config_t SENDER_FIELD = {"transfer.sender", "From", NULL, true};
const field_config_t AMOUNT_FIELD = {"transfer.amount", "Amount", format_token_amount_field, true};
const field_config_t RECEIVER_FIELD = {"transfer.receiver", "To", NULL, true};
const field_config_t INSTRUMENT_ID_FIELD = {"transfer.instrumentId.id", "Token", NULL, true};
// Transfer accept command additional fields
const field_config_t EXPIRATION_FIELD = {"transfer.executeBefore",
                                         "Expiration time",
                                         format_timestamp_field,
                                         true};
// Transfer withdraw command additional fields
const field_config_t WITHDRAW_RECEIVER_FIELD = {
    "transfer.sender",  // Sender is receiver in withdraw
    "Withdraw to",
    format_token_amount_field,
    true};
// Memo field has dots in path : escape them with backslashes (paths are stored with backslashes in
// hash table)
const field_config_t MEMO_FIELD = {
    "transfer.meta.values.splice\\.lfdecentralizedtrust\\.org/reason",
    "Memo",
    NULL,
    false};
// Native coin transfer fields
const field_config_t NATIVE_SENDER_FIELD = {"sender", "From", NULL, true};
const field_config_t NATIVE_AMOUNT_FIELD = {"amount", "Amount", format_native_amount_field, true};
const field_config_t NATIVE_RECEIVER_FIELD = {"receiver", "To", NULL, true};
const field_config_t NATIVE_MEMO_FIELD = {"description", "Memo", NULL, false};
// Pre-approval proposal fields
const field_config_t PREAPPROVAL_RECEIVER_FIELD = {"receiver",
                                                   "Pre-approve for account",
                                                   NULL,
                                                   true};
// Static field not parsed from tx but added manually in set_display_config
const field_config_t PREAPPROVAL_ASSET_FIELD = {"asset", "For asset", NULL, true};
const field_config_t PROVIDER_FIELD = {"provider", "By validator", NULL, true};

// Proxy transfer commands fields
const field_config_t PROXY_SENDER_FIELD = {"proxyArg.choiceArg.transfer.sender",
                                           "From",
                                           NULL,
                                           true};
const field_config_t PROXY_AMOUNT_FIELD = {"proxyArg.choiceArg.transfer.amount",
                                           "Amount",
                                           format_token_amount_field,
                                           true};
const field_config_t PROXY_RECEIVER_FIELD = {"proxyArg.choiceArg.transfer.receiver",
                                             "To",
                                             NULL,
                                             true};
const field_config_t PROXY_INSTRUMENT_ID_FIELD = {"proxyArg.choiceArg.transfer.instrumentId.id",
                                                  "Token",
                                                  NULL,
                                                  true};
// Memo field has dots in path : escape them with backslashes (paths are stored with backslashes in
// hash table)
const field_config_t PROXY_MEMO_FIELD = {
    "proxyArg.choiceArg.transfer.meta.values.splice\\.lfdecentralizedtrust\\.org/reason",
    "Memo",
    NULL,
    false};

static const field_config_t *const TOKEN_TRANSFER_FIELDS[TOKEN_TRANSFER_FIELDS_NB] =
    {&SENDER_FIELD, &AMOUNT_FIELD, &RECEIVER_FIELD, &INSTRUMENT_ID_FIELD, &MEMO_FIELD};

static const field_config_t *const TOKEN_TRANSFER_ACCEPT_FIELDS[TOKEN_TRANSFER_ACCEPT_FIELDS_NB] = {
    &SENDER_FIELD,
    &AMOUNT_FIELD,
    &RECEIVER_FIELD,
    &INSTRUMENT_ID_FIELD,
    &EXPIRATION_FIELD,
    &MEMO_FIELD};

static const field_config_t *const TOKEN_TRANSFER_REJECT_FIELDS[TOKEN_TRANSFER_REJECT_FIELDS_NB] = {
    &SENDER_FIELD,
    &AMOUNT_FIELD,
    &RECEIVER_FIELD,
    &INSTRUMENT_ID_FIELD,
    &EXPIRATION_FIELD,
    &MEMO_FIELD};

static const field_config_t *const
    TOKEN_TRANSFER_WITHDRAW_FIELDS[TOKEN_TRANSFER_WITHDRAW_FIELDS_NB] = {&WITHDRAW_RECEIVER_FIELD,
                                                                         &AMOUNT_FIELD,
                                                                         &INSTRUMENT_ID_FIELD};

static const field_config_t *const NATIVE_COIN_TRANSFER_FIELDS[NATIVE_TRANSFER_FIELDS_NB] = {
    &NATIVE_SENDER_FIELD,
    &NATIVE_AMOUNT_FIELD,
    &NATIVE_RECEIVER_FIELD,
    &NATIVE_MEMO_FIELD};

static const field_config_t *const PREAPPROVAL_PROPOSAL_FIELDS[PREAPPROVAL_PROPOSAL_FIELDS_NB] = {
    &PREAPPROVAL_RECEIVER_FIELD,
    &PREAPPROVAL_ASSET_FIELD,
    &PROVIDER_FIELD};

static const field_config_t *const PROXY_TRANSFER_FIELDS[PROXY_TRANSFER_FIELDS_NB] = {
    &PROXY_SENDER_FIELD,
    &PROXY_AMOUNT_FIELD,
    &PROXY_RECEIVER_FIELD,
    &PROXY_INSTRUMENT_ID_FIELD,
    &PROXY_MEMO_FIELD};

const display_config_t DISPLAY_CONFIGS[] = {
    {
        .identifier = &TOKEN_TRANSFER_ID,
        .metadata_contract_identifier = NULL,
        .fields = TOKEN_TRANSFER_FIELDS,
        .fields_count = TOKEN_TRANSFER_FIELDS_NB,
        .review_title = TOKEN_TRANSFER_REVIEW_TITLE,
        .review_finish = TOKEN_TRANSFER_REVIEW_FINISH,
    },
    {
        .identifier = &NATIVE_COIN_TRANSFER_ID,
        .metadata_contract_identifier = NULL,
        .fields = NATIVE_COIN_TRANSFER_FIELDS,
        .fields_count = NATIVE_TRANSFER_FIELDS_NB,
        .review_title = NATIVE_COIN_TRANSFER_REVIEW_TITLE,
        .review_finish = NATIVE_COIN_TRANSFER_REVIEW_FINISH,
    },
    {
        .identifier = &PREAPPROVAL_PROPOSAL_ID,
        .metadata_contract_identifier = NULL,
        .fields = PREAPPROVAL_PROPOSAL_FIELDS,
        .fields_count = PREAPPROVAL_PROPOSAL_FIELDS_NB,
        .review_title = PREAPPROVAL_PROPOSAL_REVIEW_TITLE,
        .review_finish = PREAPPROVAL_PROPOSAL_REVIEW_FINISH,
    },
    {
        .identifier = &TOKEN_TRANSFER_PROXY_ID,
        .metadata_contract_identifier = NULL,
        .fields = PROXY_TRANSFER_FIELDS,
        .fields_count = PROXY_TRANSFER_FIELDS_NB,
        .review_title = TOKEN_TRANSFER_REVIEW_TITLE,
        .review_finish = TOKEN_TRANSFER_REVIEW_FINISH,
    },
    {
        .identifier = &TOKEN_TRANSFER_ACCEPT_ID,
        .metadata_contract_identifier = &TOKEN_TRANSFER_INSTRUCTION_METADATA_ID,
        .fields = TOKEN_TRANSFER_ACCEPT_FIELDS,
        .fields_count = TOKEN_TRANSFER_ACCEPT_FIELDS_NB,
        .review_title = TOKEN_TRANSFER_ACCEPT_REVIEW_TITLE,
        .review_finish = TOKEN_TRANSFER_ACCEPT_REVIEW_FINISH,
    },
    {
        .identifier = &TOKEN_TRANSFER_REJECT_ID,
        .metadata_contract_identifier = &TOKEN_TRANSFER_INSTRUCTION_METADATA_ID,
        .fields = TOKEN_TRANSFER_REJECT_FIELDS,
        .fields_count = TOKEN_TRANSFER_REJECT_FIELDS_NB,
        .review_title = TOKEN_TRANSFER_REJECT_REVIEW_TITLE,
        .review_finish = TOKEN_TRANSFER_REJECT_REVIEW_FINISH,
    },
    {
        .identifier = &TOKEN_TRANSFER_WITHDRAW_ID,
        .metadata_contract_identifier = &TOKEN_TRANSFER_INSTRUCTION_METADATA_ID,
        .fields = TOKEN_TRANSFER_WITHDRAW_FIELDS,
        .fields_count = TOKEN_TRANSFER_WITHDRAW_FIELDS_NB,
        .review_title = TOKEN_TRANSFER_WITHDRAW_REVIEW_TITLE,
        .review_finish = TOKEN_TRANSFER_WITHDRAW_REVIEW_FINISH,
    },
};

static tx_field_t tx_fields[MAX_DISPLAY_FIELDS_NB];
static identifier_config_t *global_tx_metadata_contract_identifier = NULL;
static display_config_t *global_tx_metadata_display_conf = NULL;

/* -------------------------------------------------------------------------- */
/*  Field formatting callbacks                                                */
/* -------------------------------------------------------------------------- */

// Remove all trailing zeros and possible dot if integer for amount fields
static void format_amount_field(pb_callback_context_t *ctx, tx_field_t *field) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to format_amount_field");
    LEDGER_ASSERT(field != NULL, "NULL field passed to format_amount_field");
    UNUSED(ctx);

    if (field->value == NULL) {
        PRINTF("Value is NULL, skipping formatting\n");
        return;
    }

    size_t len = strlen(field->value);

    if (len == 0) {
        PRINTF("Value is empty, skipping formatting\n");
        return;
    }

    PRINTF("Formatting amount field with value: %s\n", field->value);

    char *dot = strchr(field->value, '.');
    if (dot != NULL) {
        char *end = field->value + len - 1;
        while (end > dot && *end == '0') {
            *end-- = '\0';
        }
        if (end == dot) {
            *end = '\0';  // Remove the dot if it's the last character
        }
    }

    PRINTF("Formatted amount: %s\n", field->value);
}

static void format_token_amount_field(pb_callback_context_t *ctx, tx_field_t *field) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to format_token_amount_field");
    LEDGER_ASSERT(field != NULL, "NULL field passed to format_token_amount_field");

    // Call the generic amount formatter first
    format_amount_field(ctx, field);

    // Find the instrument ID field dynamically
    tx_field_t *instrument_field = NULL;
    for (size_t i = 0; i < ctx->nb_fields; i++) {
        const char *path = (const char *) PIC(ctx->tx_fields[i].config->path);
        if (strcmp(path, (const char *) PIC(INSTRUMENT_ID_FIELD.path)) == 0 ||
            strcmp(path, (const char *) PIC(PROXY_INSTRUMENT_ID_FIELD.path)) == 0) {
            instrument_field = &ctx->tx_fields[i];
            break;
        }
    }

    if (instrument_field == NULL || !instrument_field->found || instrument_field->value == NULL) {
        return;
    }

    // Look for the instrument id in the mapping to add the ticker if found
    for (size_t i = 0; i < sizeof(INSTRUMENT_ID_TO_TICKER_MAPPING) / (2 * sizeof(char *)); i++) {
        const char *instrument_id = (const char *) PIC(INSTRUMENT_ID_TO_TICKER_MAPPING[2 * i]);
        const char *ticker = (const char *) PIC(INSTRUMENT_ID_TO_TICKER_MAPPING[2 * i + 1]);
        // Check if stored instrument id in available display items matches
        if (strcmp(instrument_field->value, instrument_id) == 0) {
            // Append ticker to value
            size_t new_len = strlen(field->value) + 1 + strlen(ticker) + 1;
            char *new_value = (char *) app_mem_alloc(new_len);
            if (new_value == NULL) {
                PRINTF("Memory allocation failed in format_token_amount_field\n");
                return;
            }
            SNPRINTF(new_value, new_len, "%s %s", field->value, ticker);
            app_mem_free(field->value);
            field->value = new_value;
            field->value_len = new_len;

            // If matched no need to display the instrument id field
            instrument_field->display = false;

            // If matched "Amulet" update review title and finish to mention Canton Coin
            if (strcmp(ticker, NATIVE_COIN_TICKER) == 0 &&
                strcmp((const char *) PIC(ctx->review_title), TOKEN_TRANSFER_REVIEW_TITLE) == 0) {
                ctx->review_title = NATIVE_COIN_TRANSFER_REVIEW_TITLE;
                ctx->review_finish = NATIVE_COIN_TRANSFER_REVIEW_FINISH;
            }

            break;
        }
    }
}

static void format_native_amount_field(pb_callback_context_t *ctx, tx_field_t *field) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to format_native_amount_field");
    LEDGER_ASSERT(field != NULL, "NULL field passed to format_native_amount_field");

    // Call the generic amount formatter first
    format_amount_field(ctx, field);
    // Append "CC" ticker for Canton Coin
    size_t new_len = strlen(field->value) + 1 + strlen(NATIVE_COIN_TICKER) + 1;
    // Allocate new string
    char *new_value = (char *) app_mem_alloc(new_len);
    if (new_value == NULL) {
        PRINTF("Memory allocation failed in format_native_amount_field\n");
        return;
    }
    // Format the new value
    SNPRINTF(new_value, new_len, "%s %s", field->value, NATIVE_COIN_TICKER);

    // Free old value and update pointer
    app_mem_free(field->value);
    field->value = new_value;
    field->value_len = new_len;
}

static void format_timestamp_field(pb_callback_context_t *ctx, tx_field_t *field) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to format_timestamp_field");
    LEDGER_ASSERT(field != NULL, "NULL field passed to format_timestamp_field");
    UNUSED(ctx);

    if (field->value == NULL) {
        return;
    }

    uint64_t ts_micros = read_u64_be((uint8_t *) field->value, 0);
    time_t timestamp = (time_t) (ts_micros / 1000000ULL);  // Convert microseconds to seconds
    struct tm tm_info;
    if (gmtime_r(&timestamp, &tm_info) == NULL) {
        return;
    }

    // Prepare 12-hour format components
    int hour = tm_info.tm_hour % 12;
    if (hour == 0) hour = 12;
    const char *am_pm = (tm_info.tm_hour >= 12) ? "PM" : "AM";

    // Allocate and format
    size_t buffer_size = 64;
    char *new_value = (char *) app_mem_alloc(buffer_size);
    if (new_value == NULL) {
        PRINTF("Memory allocation failed in format_timestamp_field\n");
        return;
    }

#ifdef SCREEN_SIZE_WALLET
#define TIMESTAMP_FMT "%04d-%02d-%02d %02d:%02d:%02d\n%s UTC"
#else
#define TIMESTAMP_FMT "%04d-%02d-%02d\n%02d:%02d:%02d %s UTC"
#endif

    SNPRINTF(new_value,
             buffer_size,
             TIMESTAMP_FMT,
             tm_info.tm_year + 1900,
             tm_info.tm_mon + 1,
             tm_info.tm_mday,
             hour,
             tm_info.tm_min,
             tm_info.tm_sec,
             am_pm);

#undef TIMESTAMP_FMT

    // Update field
    app_mem_free(field->value);
    field->value = new_value;
    field->value_len = strlen(new_value) + 1;

    PRINTF("Formatted timestamp: %s\n", field->value);
}

/* -------------------------------------------------------------------------- */
/*  Helper functions for transaction display management                       */
/* -------------------------------------------------------------------------- */

// Helper function to find field state by path
MUST_CHECK static tx_field_t *find_field_by_path(pb_callback_context_t *ctx, const char *path) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to find_field_by_path");
    LEDGER_ASSERT(path != NULL, "NULL path passed to find_field_by_path");

    if (ctx->tx_fields == NULL) return NULL;

    for (size_t i = 0; i < ctx->nb_fields; i++) {
        const char *field_path = (const char *) PIC(ctx->tx_fields[i].config->path);
        if (strcmp(field_path, path) == 0) {
            return &ctx->tx_fields[i];
        }
    }
    return NULL;
}

// Helper function to initialize transaction pairs used for display
MUST_CHECK bool init_transaction_pairs(transaction_ctx_t *tx_info, size_t count) {
    LEDGER_ASSERT(tx_info != NULL, "NULL transaction context passed to init_transaction_pairs");

    // Free existing pairs if any
    if (tx_info->pairs != NULL) {
        app_mem_free(tx_info->pairs);
    }

    // Free existing allocated items strings if any
    if (tx_info->display_items_strings != NULL) {
        // Free individual strings first
        for (size_t i = 0; i < tx_info->pairs_count; i++) {
            if (tx_info->display_items_strings[i] != NULL) {
                app_mem_free(tx_info->display_items_strings[i]);
            }
        }
        app_mem_free(tx_info->display_items_strings);
    }

    // Allocate new arrays
    tx_info->pairs_count = 0;
    tx_info->pairs =
        (nbgl_contentTagValue_t *) app_mem_alloc(count * sizeof(nbgl_contentTagValue_t));
    tx_info->display_items_strings = (char **) app_mem_alloc(count * sizeof(char *));

    if (tx_info->pairs == NULL || tx_info->display_items_strings == NULL) {
        return false;
    }

    memset(tx_info->pairs, 0, count * sizeof(nbgl_contentTagValue_t));
    memset(tx_info->display_items_strings, 0, count * sizeof(char *));

    return true;
}

// Helper function to set field value safely with context-managed memory
static void set_field_value(tx_field_t *field_state, const void *value, pb_size_t value_type) {
    LEDGER_ASSERT(field_state != NULL, "NULL field state passed to set_field_value");
    LEDGER_ASSERT(value != NULL, "NULL value passed to set_field_value");

    const void *src = NULL;
    size_t len = 0;
    uint8_t ts_buf[8];

    if (value_type == VALUE_TIMESTAMP_TAG) {
        ByteWriter bw;
        bw_init(&bw, ts_buf, sizeof(ts_buf));
        bw_put_u64_be(&bw, (uint64_t) * ((int64_t *) PIC(value)));
        src = ts_buf;
        len = sizeof(ts_buf);
    } else {
        src = PIC(value);
        len = strlen((const char *) src);
        if (len == 0) {
            PRINTF("Empty value passed to set_field_value, skipping\n");
            return;
        }
        len++;  // Include null terminator
    }

    field_state->value = (char *) app_mem_alloc(len);
    if (field_state->value != NULL) {
        memcpy(field_state->value, src, len);
        field_state->value_len = len;
        field_state->found = true;
    }
}

// Helper function to set display configuration from a const array.
static void set_display_config(pb_callback_context_t *ctx, const display_config_t *config_source) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to set_display_config");
    LEDGER_ASSERT(config_source != NULL, "NULL config source passed to set_display_config");

    PRINTF("Setting display config with %d fields\n", config_source->fields_count);

    const field_config_t *const *source =
        (const field_config_t *const *) PIC(config_source->fields);

    ctx->nb_fields = config_source->fields_count;
    ctx->review_title = config_source->review_title;
    ctx->review_finish = config_source->review_finish;

    // Initialize field states
    for (size_t i = 0; i < ctx->nb_fields; i++) {
        tx_fields[i].config = (const field_config_t *) PIC(source[i]);
        tx_fields[i].found = false;
        tx_fields[i].display = true;
    }

    ctx->tx_fields = tx_fields;

    LEDGER_ASSERT(init_transaction_pairs(ctx->tx_info, ctx->nb_fields) == true,
                  "Failed to initialize transaction pairs");

    // If pre-approval proposal, add static field for asset
    if (strcmp((const char *) PIC(ctx->review_title), PREAPPROVAL_PROPOSAL_REVIEW_TITLE) == 0) {
        tx_field_t *field_state = &ctx->tx_fields[PREAPPROVAL_ASSET_FIELD_INDEX];
        field_state->config = (const field_config_t *) PIC(&PREAPPROVAL_ASSET_FIELD);
        field_state->found = true;
        set_field_value(field_state, PREAPPROVAL_ASSET_FIELD_VALUE, VALUE_TEXT_TAG);
    }

    return;
}

// Helper function to match identifiers
MUST_CHECK static bool match_identifier(const Identifier *id, const identifier_config_t *config) {
    LEDGER_ASSERT(id != NULL, "NULL identifier passed to match_identifier");
    LEDGER_ASSERT(config != NULL, "NULL config passed to match_identifier");

    return strcmp(id->module_name, (char *) PIC(config->module_name)) == 0 &&
           strcmp(id->entity_name, (char *) PIC(config->entity_name)) == 0;
}

// Identify transaction type and set display configuration accordingly
static void find_tx_type_and_config(pb_callback_context_t *ctx, const Identifier *id) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to find_tx_type_and_config");
    LEDGER_ASSERT(id != NULL, "NULL identifier passed to find_tx_type_and_config");

    if (ctx->tx_fields != NULL) {
        // Already set, no need to find again
        return;
    }

    if (global_tx_metadata_contract_identifier != NULL) {
        // Check if current id matches metadata contract identifier
        if (match_identifier(
                id,
                (const identifier_config_t *) PIC(global_tx_metadata_contract_identifier))) {
            set_display_config(ctx, global_tx_metadata_display_conf);
        }
        return;
    }

    for (size_t i = 0; i < sizeof(DISPLAY_CONFIGS) / sizeof(DISPLAY_CONFIGS[0]); i++) {
        const display_config_t *config = (const display_config_t *) PIC(&DISPLAY_CONFIGS[i]);
        if (match_identifier(id, (const identifier_config_t *) PIC(config->identifier))) {
            // If metadata contract identifier is set, set it in global transaction context for
            // later use and do not set display config yet
            if (config->metadata_contract_identifier != NULL) {
                global_tx_metadata_contract_identifier =
                    (identifier_config_t *) PIC(config->metadata_contract_identifier);
                global_tx_metadata_display_conf = (display_config_t *) PIC(config);
                PRINTF("Metadata contract identifier set, deferring display config setting\n");
                return;
            }
            set_display_config(ctx, config);
            return;
        }
    }
    return;
}

// Lookup field in hash map and set value for display if found. Discriminate field types if needed.
static void find_tx_field(pb_callback_context_t *ctx, cbValue *value) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to find_tx_field");
    LEDGER_ASSERT(value != NULL, "NULL value passed to find_tx_field");

    if (ctx->tx_fields != NULL) {
        PRINTF("Looking up field path: %s\n", ctx->field_path);
        tx_field_t *state = find_field_by_path(ctx, ctx->field_path);
        if (state != NULL && value != NULL) {
            PRINTF("Found matching field for path: %s\n", ctx->field_path);
            // Set the field value
            switch (value->which_sum) {
                case VALUE_PARTY_TAG:
                    PRINTF("Setting party value: %s\n", value->party);
                    set_field_value(state, value->party, value->which_sum);
                    break;
                case VALUE_NUMERIC_TAG:
                    PRINTF("Setting numeric value: %s\n", value->numeric);
                    set_field_value(state, value->numeric, value->which_sum);
                    break;
                case VALUE_TEXT_TAG:
                    PRINTF("Setting text value: %s\n", value->text);
                    set_field_value(state, value->text, value->which_sum);
                    break;
                case VALUE_TIMESTAMP_TAG:
                    // PRINTF("Setting timestamp value: %lld\n", (long long) value->timestamp);
                    set_field_value(state, (void *) &value->timestamp, value->which_sum);
                    break;
                default:
                    PRINTF("Field type not handled for display: %d\n", value->which_sum);
                    break;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  Field path management                                                     */
/* -------------------------------------------------------------------------- */

// Function to allocate memory for the display field path
static void init_field_path(pb_callback_context_t *ctx) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to init_field_path");

    if (ctx->field_path != NULL) {
        app_mem_free(ctx->field_path);
    }
    ctx->field_path = (char *) app_mem_alloc(MAX_FIELD_PATH_LEN);
    memset(ctx->field_path, 0, MAX_FIELD_PATH_LEN);
}

// Function to free memory for the display field path
static void free_field_path(pb_callback_context_t *ctx) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to free_field_path");

    if (ctx->field_path != NULL) {
        app_mem_free(ctx->field_path);
        ctx->field_path = NULL;
    }
}

// Push a new segment onto the field path with escaping for dots
static void push_path(pb_callback_context_t *ctx, const char *new_segment) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to push_path");
    // new_segment can be NULL (early return)

    if (!ctx->tx_fields || !ctx->field_path || !new_segment) return;

    size_t current_len = strlen(ctx->field_path);
    size_t new_len = 0;

    // Count length with escaping
    for (size_t i = 0; new_segment[i]; i++) {
        if (new_segment[i] == '.') new_len++;  // escape
        new_len++;
    }

    if (current_len + 1 + new_len >= MAX_FIELD_PATH_LEN) return;

    // Add dot separator if needed
    if (current_len > 0) ctx->field_path[current_len++] = '.';

    // Copy new segment with escaping
    for (size_t i = 0; new_segment[i]; i++) {
        if (new_segment[i] == '.') ctx->field_path[current_len++] = '\\';
        ctx->field_path[current_len++] = new_segment[i];
    }

    ctx->field_path[current_len] = '\0';
}

// Pop the last segment from the field path considering escaping
static void pop_path(pb_callback_context_t *ctx) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to pop_path");

    if (!ctx->tx_fields || !ctx->field_path) return;

    // Find last unescaped dot
    char *p = ctx->field_path + strlen(ctx->field_path) - 1;
    while (p >= ctx->field_path) {
        if (*p == '.' && (p == ctx->field_path || *(p - 1) != '\\')) {
            *p = '\0';
            return;
        }
        p--;
    }
    ctx->field_path[0] = '\0';
}

/* -------------------------------------------------------------------------- */
/*  Protobuf decoding callbacks                                               */
/* -------------------------------------------------------------------------- */

// Decode the label field of a record field
MUST_CHECK static bool decode_record_field_label(pb_istream_t *stream,
                                                 const pb_field_t *field,
                                                 void **arg) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_record_field_label");
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to decode_record_field_label");
    UNUSED(field);

    pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);
    // Read string from stream
    char label_buffer[DEFAULT_DECODE_BUFFER_SIZE] = {0};

    size_t len =
        stream->bytes_left < sizeof(label_buffer) ? stream->bytes_left : sizeof(label_buffer) - 1;

    if (!pb_read(stream, (pb_byte_t *) label_buffer, len)) {
        PRINTF("Failed to read string from stream\n");
        return false;
    }
    // Push the label onto the field path
    push_path(ctx, label_buffer);
    return true;
}

// Decode the record ID field to identify the record type
MUST_CHECK static bool decode_record_id_field(pb_istream_t *stream,
                                              const pb_field_t *field,
                                              void **arg) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_record_id_field");
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to decode_record_id_field");
    UNUSED(field);

    pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);
    com_daml_ledger_api_v2_Identifier id = {};

    if (!pb_decode(stream, com_daml_ledger_api_v2_Identifier_fields, &id)) {
        PRINTF("Failed to decode Record ID field\n");
        return false;
    }

    // Identify the kind of record with match_identifier to know if we should
    // process fields for display.
    find_tx_type_and_config(ctx, &id);

    pb_release(com_daml_ledger_api_v2_Identifier_fields, &id);

    return true;
}

// Decode a record field, pushing and popping the field path as needed
MUST_CHECK static bool decode_record_field(pb_istream_t *stream,
                                           const pb_field_t *field,
                                           void **arg) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_record_field");
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to decode_record_field");
    UNUSED(field);

    pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);

    if (ctx->tx_fields == NULL) {
        // No display configuration set, skip decoding
        return true;
    }

    PRINTF("Decoding Record field\n");
    cbRecordField rf = {};
    rf.label.funcs.decode = &decode_record_field_label;
    rf.label.arg = ctx;

    rf.value.cb_sum.funcs.decode = &decode_value_variant;
    rf.value.cb_sum.arg = ctx;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_RecordField_fields, &rf)) {
        PRINTF("Failed to decode Record field: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    // Check if current field path matches any display configured field
    find_tx_field(ctx, &rf.value);

    pop_path(ctx);

    pb_release(com_daml_ledger_api_v2_cb_RecordField_fields, &rf);

    return true;
}

// Decode an Optional value, which may contain another Value
MUST_CHECK static bool decode_value(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_value");
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to decode_value");
    UNUSED(field);

    pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);

    PRINTF("Decoding Optional value\n");

    cbValue v = com_daml_ledger_api_v2_cb_Value_init_zero;
    v.cb_sum.funcs.decode = &decode_value_variant;
    v.cb_sum.arg = ctx;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Value_fields, &v)) {
        PRINTF("Failed to decode Optional value: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    find_tx_field(ctx, &v);

    pb_release(com_daml_ledger_api_v2_cb_Value_fields, &v);

    return true;
}

MUST_CHECK static bool decode_textmap_key(pb_istream_t *stream,
                                          const pb_field_t *field,
                                          void **arg) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_textmap_key");
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to decode_textmap_key");
    UNUSED(field);

    pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);

    // Read string from stream
    char key_buffer[DEFAULT_DECODE_BUFFER_SIZE] = {0};
    size_t len =
        stream->bytes_left < sizeof(key_buffer) ? stream->bytes_left : sizeof(key_buffer) - 1;

    if (!pb_read(stream, (pb_byte_t *) key_buffer, len)) {
        PRINTF("Failed to read string from stream\n");
        return false;
    }

    PRINTF("Decoded TextMap key: %s\n", key_buffer);

    // Push entry key onto path
    push_path(ctx, key_buffer);

    return true;
}

MUST_CHECK static bool decode_value_text_map(pb_istream_t *stream,
                                             const pb_field_t *field,
                                             void **arg) {
    UNUSED(field);
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_value_text_map");
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to decode_value_text_map");

    pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);

    PRINTF("Decoding TextMap value\n");

    cbTextMapEntry entry = com_daml_ledger_api_v2_cb_TextMap_Entry_init_zero;

    entry.key.funcs.decode = &decode_textmap_key;
    entry.key.arg = ctx;
    entry.value.cb_sum.funcs.decode = &decode_value_variant;
    entry.value.cb_sum.arg = ctx;

    if (!pb_decode(stream, com_daml_ledger_api_v2_cb_TextMap_Entry_fields, &entry)) {
        PRINTF("Failed to decode TextMap entry: %s\n", PB_GET_ERROR(stream));
        return false;
    }

    // Check if current field path matches any display configured field
    find_tx_field(ctx, &entry.value);

    // Pop entry key from path
    pop_path(ctx);

    pb_release(com_daml_ledger_api_v2_cb_TextMap_Entry_fields, &entry);

    return true;
}

// Decode a value variant, handling Record types specifically
MUST_CHECK static bool decode_value_variant(pb_istream_t *stream,
                                            const pb_field_t *field,
                                            void **arg) {
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_value_variant");
    LEDGER_ASSERT(field != NULL, "NULL field passed to decode_value_variant");
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to decode_value_variant");

    cbValue *topmsg = field->message;
    (void) topmsg;

    switch (field->tag) {
        case com_daml_ledger_api_v2_cb_Value_record_tag: {
            pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);
            cbRecord *msg = field->pData;
            msg->fields.funcs.decode = &decode_record_field;
            msg->fields.arg = ctx;
            msg->record_id.funcs.decode = &decode_record_id_field;
            msg->record_id.arg = ctx;
            // decode the record, which will invoke the callbacks
            if (!pb_decode(stream, com_daml_ledger_api_v2_cb_Record_fields, msg)) {
                PRINTF("Failed to decode Record in Value: %s\n", PB_GET_ERROR(stream));
                return false;
            }
            pb_release(com_daml_ledger_api_v2_cb_Record_fields, msg);
            break;
        }
        case com_daml_ledger_api_v2_cb_Value_optional_tag: {
            pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);
            cbOptional *msg = field->pData;
            msg->value.funcs.decode = &decode_value;
            msg->value.arg = ctx;
            break;
        }
        case com_daml_ledger_api_v2_cb_Value_text_map_tag: {
            pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);
            cbTextMap *msg = field->pData;
            msg->entries.funcs.decode = &decode_value_text_map;
            msg->entries.arg = ctx;
            break;
        }
        default: {
            // For other types, no special handling needed here
            break;
        }
    }
    return true;
}

// Callback to decode Node messages, focusing on Exercise and Create nodes
static bool node_decode_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    UNUSED(stream);
    LEDGER_ASSERT(field != NULL, "NULL field passed to node_decode_callback");
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to node_decode_callback");

    pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);
    com_daml_ledger_api_v2_interactive_transaction_v1_cb_NodeDisplay *node = field->message;

    // Only handle Exercise nodes
    if (field->tag == NODE_V1_EXERCISE_TAG) {
        PRINTF("Decoding Exercise node\n");
        node->exercise.chosen_value.cb_sum.funcs.decode = &decode_value_variant;
        node->exercise.chosen_value.cb_sum.arg = ctx;
    } else if (field->tag == NODE_V1_CREATE_TAG) {
        PRINTF("Decoding Create node\n");
        node->create.argument.cb_sum.funcs.decode = &decode_value_variant;
        node->create.argument.cb_sum.arg = ctx;
    }

    return true;
}

// Callback to decode versioned node messages, attaching Node-level callback
MUST_CHECK static bool versioned_node_decode_callback(pb_istream_t *stream,
                                                      const pb_field_t *field,
                                                      void **arg) {
    UNUSED(stream);
    LEDGER_ASSERT(field != NULL, "NULL field passed to versioned_node_decode_callback");
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to versioned_node_decode_callback");

    pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);
    com_daml_ledger_api_v2_interactive_DeviceDamlTransactionDisplay_Node *node = field->message;

    // Attach Node-level callback dynamically
    if (node->NODE_VERSION_ONEOF_FIELD == NODE_V1_TAG) {
        node->v1.cb_node_type.funcs.decode = &node_decode_callback;
        node->v1.cb_node_type.arg = ctx;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  Entry point for parsing transaction display information                   */
/* -------------------------------------------------------------------------- */

MUST_CHECK int format_and_populate_display_items(pb_callback_context_t *ctx) {
    LEDGER_ASSERT(ctx != NULL, "NULL context passed to format_and_populate_display_items");

    // Loop for mandatory check + format callbacks
    for (size_t i = 0; i < ctx->nb_fields; i++) {
        const tx_field_t *state = &ctx->tx_fields[i];
        const field_config_t *cfg = state->config;

        // Mandatory field check
        if (cfg->mandatory && !state->found) {
            PRINTF("Mandatory field not found: %s\n", (char *) PIC(cfg->path));
            G_context.tx_info.clear_signing_available = false;
            return -1;
        }

        // Execute formatting callback if applicable
        if (state->found && state->display) {
            field_format_callback_t callback =
                (field_format_callback_t) PIC(ctx->tx_fields[i].config->format_callback);
            if (callback != NULL) {
                callback(ctx, (void *) &state->value);
            }
        }
    }

    // Second loop: populate display items
    uint8_t idx = 0;
    ctx->tx_info->pairs_count = 0;
    for (size_t i = 0; i < ctx->nb_fields; i++) {
        tx_field_t *state = &ctx->tx_fields[i];
        if (state->found && state->display && state->value_len > 0) {
            char *dst = app_mem_alloc(state->value_len);
            if (dst != NULL) {
                memcpy(dst, state->value, state->value_len);

                ctx->tx_info->display_items_strings[idx] = dst;
                ctx->tx_info->pairs[idx].item = (char *) PIC(state->config->item_name);
                ctx->tx_info->pairs[idx].value = dst;

                idx++;
                ctx->tx_info->pairs_count++;

                app_mem_free(state->value);
                state->value = NULL;
                state->value_len = 0;
            }
        }
    }
    return 0;
}

MUST_CHECK int parse_node_for_display(buffer_t *buf) {
    LEDGER_ASSERT(buf != NULL, "NULL buffer passed to parse_node_for_display");

    // Only parse if we haven't already found all fields or if no metadata contract identifier is
    // set
    if (G_context.tx_info.clear_signing_available ||
        global_tx_metadata_contract_identifier != NULL) {
        return 0;
    }

    pb_callback_context_t ctx = {0};
    init_field_path(&ctx);
    ctx.tx_info = &G_context.tx_info;
    ctx.tx_fields = NULL;
    global_tx_metadata_contract_identifier = NULL;

    G_context.tx_info.tx_parts_ctx.node.cb_versioned_node.funcs.decode =
        &versioned_node_decode_callback;
    G_context.tx_info.tx_parts_ctx.node.cb_versioned_node.arg = &ctx;

    pb_istream_t stream = pb_istream_from_buffer(buf->ptr, buf->size);

    if (!pb_decode(&stream,
                   com_daml_ledger_api_v2_interactive_DeviceDamlTransactionDisplay_Node_fields,
                   &G_context.tx_info.tx_parts_ctx.node)) {
        PRINTF("Decode failed: %s\n", PB_GET_ERROR(&stream));
        return -1;
    }

    pb_release(com_daml_ledger_api_v2_interactive_DeviceDamlTransactionDisplay_Node_fields,
               &G_context.tx_info.tx_parts_ctx.node);

    free_field_path(&ctx);

    G_context.tx_info.tx_parts_ctx.node.cb_versioned_node.funcs.decode = NULL;
    G_context.tx_info.tx_parts_ctx.node.cb_versioned_node.arg = NULL;

    if (ctx.tx_fields == NULL) {
        PRINTF("No display configuration set during parsing, skipping display population\n");
        G_context.tx_info.clear_signing_available = false;
        return 0;
    }

    if (format_and_populate_display_items(&ctx) != 0) {
        return -1;
    }

    G_context.tx_info.clear_signing_available = true;
    G_context.tx_info.review_title = ctx.review_title;
    G_context.tx_info.review_finish = ctx.review_finish;

    return 0;
}

MUST_CHECK static bool decode_create(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    (void) field;
    LEDGER_ASSERT(arg != NULL, "NULL arg passed to decode_create");
    LEDGER_ASSERT(stream != NULL, "NULL stream passed to decode_create");

    PRINTF("Decode Create node\n");

    pb_callback_context_t *ctx = (pb_callback_context_t *) (*arg);

    com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create c_cb =
        com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_init_zero;

    // Decoding Create node CB recursive field `argument` and hashing it inside callbacks
    c_cb.argument.funcs.decode = &decode_value;
    c_cb.argument.arg = ctx;

    if (!pb_decode(stream,
                   com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields,
                   &c_cb)) {
        PRINTF("Failed to decode Create node (CB): %s\n", PB_GET_ERROR(stream));
        return false;
    }

    pb_release(com_daml_ledger_api_v2_interactive_transaction_v1_cb_Create_fields, &c_cb);

    PRINTF("/Decode Create node\n");

    return true;
}

MUST_CHECK int parse_input_contract_for_display(buffer_t *buf) {
    LEDGER_ASSERT(buf != NULL, "NULL buffer passed to proto_deserialize_input_contract");

    if (G_context.tx_info.clear_signing_available ||
        global_tx_metadata_contract_identifier == NULL) {
        return 0;
    }

    pb_callback_context_t ctx = {0};
    init_field_path(&ctx);
    ctx.tx_info = &G_context.tx_info;
    ctx.tx_fields = NULL;

    pb_istream_t stream = pb_istream_from_buffer(buf->ptr, buf->size);

    PRINTF("Decoding Input contract from buffer of size %d bytes\n", buf->size);

    G_context.tx_info.tx_parts_ctx.input_contract.cb_contract.funcs.decode = &decode_create;
    G_context.tx_info.tx_parts_ctx.input_contract.cb_contract.arg = &ctx;

    if (!pb_decode(&stream,
                   com_daml_ledger_api_v2_interactive_DeviceMetadata_InputContract_fields,
                   &G_context.tx_info.tx_parts_ctx.input_contract)) {
        PRINTF("Failed to decode Input contract: %s\n", PB_GET_ERROR(&stream));
        return -1;
    }

    pb_release(com_daml_ledger_api_v2_interactive_DeviceMetadata_InputContract_fields,
               &G_context.tx_info.tx_parts_ctx.input_contract);

    free_field_path(&ctx);

    G_context.tx_info.tx_parts_ctx.node.cb_versioned_node.funcs.decode = NULL;
    G_context.tx_info.tx_parts_ctx.node.cb_versioned_node.arg = NULL;

    if (ctx.tx_fields == NULL) {
        PRINTF("No display configuration set during parsing, skipping display population\n");
        G_context.tx_info.clear_signing_available = false;
        return 0;
    }

    if (format_and_populate_display_items(&ctx) != 0) {
        return -1;
    }

    G_context.tx_info.clear_signing_available = true;
    G_context.tx_info.review_title = ctx.review_title;
    G_context.tx_info.review_finish = ctx.review_finish;

    return 0;
}
