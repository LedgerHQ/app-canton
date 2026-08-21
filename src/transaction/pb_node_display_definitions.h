#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "types.h"

/* -------------------------------------------------------------------------- */
/* Constants                                                                  */
/* -------------------------------------------------------------------------- */

#define MAX_DISPLAY_FIELDS_NB 10
#define MAX_FIELD_PATH_LEN    128

#define PREAPPROVAL_ASSET_FIELD_INDEX 1

#define NATIVE_COIN_TICKER            "CC"
#define NATIVE_COIN_INSTRUMENT_ID     "Amulet"
#define PREAPPROVAL_ASSET_FIELD_VALUE "Canton Coin (CC)"

/* Review Strings Parts */
#define STR_Review_Tx          "Review transaction"
#define STR_Sign_Tx            "Sign transaction"
#define STR_To_Send_Tokens     "to send tokens"
#define STR_To_Accept_Transfer "to accept incoming transfer"
#define STR_To_Reject_Transfer "to reject incoming transfer"
#define STR_To_Withdraw_Offer  "to withdraw transfer offer"
#define STR_To_Send_CC         "to send Canton Coin"
#define STR_To_Preapprove      "to pre-approve incoming transfers"

/* * Full Title Macros
 * (Required by pb_node_display_parser.c logic for comparisons)
 */
#define TOKEN_TRANSFER_REVIEW_TITLE           STR_Review_Tx " " STR_To_Send_Tokens
#define TOKEN_TRANSFER_REVIEW_FINISH          STR_Sign_Tx " " STR_To_Send_Tokens "?"
#define TOKEN_TRANSFER_ACCEPT_REVIEW_TITLE    STR_Review_Tx " " STR_To_Accept_Transfer
#define TOKEN_TRANSFER_ACCEPT_REVIEW_FINISH   STR_Sign_Tx " " STR_To_Accept_Transfer "?"
#define TOKEN_TRANSFER_REJECT_REVIEW_TITLE    STR_Review_Tx " " STR_To_Reject_Transfer
#define TOKEN_TRANSFER_REJECT_REVIEW_FINISH   STR_Sign_Tx " " STR_To_Reject_Transfer "?"
#define TOKEN_TRANSFER_WITHDRAW_REVIEW_TITLE  STR_Review_Tx " " STR_To_Withdraw_Offer
#define TOKEN_TRANSFER_WITHDRAW_REVIEW_FINISH STR_Sign_Tx " " STR_To_Withdraw_Offer "?"
#define NATIVE_COIN_TRANSFER_REVIEW_TITLE     STR_Review_Tx " " STR_To_Send_CC
#define NATIVE_COIN_TRANSFER_REVIEW_FINISH    STR_Sign_Tx " " STR_To_Send_CC "?"
#define PREAPPROVAL_PROPOSAL_REVIEW_TITLE     STR_Review_Tx " " STR_To_Preapprove
#define PREAPPROVAL_PROPOSAL_REVIEW_FINISH    STR_Sign_Tx " " STR_To_Preapprove "?"

/* -------------------------------------------------------------------------- */
/* Type Definitions                                                           */
/* -------------------------------------------------------------------------- */

typedef struct pb_callback_context_t pb_callback_context_t;
typedef struct tx_field_t tx_field_t;
typedef void (*field_format_callback_t)(pb_callback_context_t *ctx, tx_field_t *field);

typedef struct {
    const char *admin;
    const char *id;
    const char *ticker;
} instrument_to_ticker_mapping_t;

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
    char *value;
    size_t value_len;
    const field_config_t *config;
    bool found;
    bool display;
};

typedef struct {
    identifier_config_t identifier;
    const field_config_t *fields;
    size_t fields_count;
    const char *review_title;
    const char *review_finish;
    const identifier_config_t *const *metadata_contract_identifiers;
    const size_t metadata_contract_identifiers_count;
} display_config_t;

struct pb_callback_context_t {
    char *field_path;
    transaction_ctx_t *tx_info;
    tx_field_t *tx_fields;
    uint8_t nb_fields;
    const char *review_title;
    const char *review_finish;
    bool unknown_token;
};

/* -------------------------------------------------------------------------- */
/* External Declarations                                                      */
/* -------------------------------------------------------------------------- */

void format_token_amount_field(pb_callback_context_t *ctx, tx_field_t *field);
void format_native_amount_field(pb_callback_context_t *ctx, tx_field_t *field);
void format_timestamp_field(pb_callback_context_t *ctx, tx_field_t *field);

extern const display_config_t DISPLAY_CONFIGS[];
extern const size_t DISPLAY_CONFIGS_NB;

extern const field_config_t INSTRUMENT_ID_FIELD;
extern const field_config_t INSTRUMENT_ID_ADMIN_FIELD;
extern const field_config_t INSTRUMENT_ID_PROXY_ADMIN_FIELD;
extern const field_config_t PROXY_INSTRUMENT_ID_FIELD;
extern const field_config_t PREAPPROVAL_ASSET_FIELD;

extern const instrument_to_ticker_mapping_t INSTRUMENT_TO_TICKER_MAPPINGS[];
extern const size_t INSTRUMENT_TO_TICKER_MAPPING_NB;
