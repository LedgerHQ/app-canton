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

/**
 * What a displayed field means to the value check.
 *
 * The path says where to read a field and the item name says how to label it, but neither says what
 * it is. This does, so each path stays written once.
 */
typedef enum {
    SHOWN_AS_OTHER = 0,  // nothing the value check compares
    SHOWN_AS_SENDER,
    SHOWN_AS_RECEIVER,
    SHOWN_AS_AMOUNT,
    SHOWN_AS_ADMIN,       // the instrument admin the displayed ticker was resolved from
    SHOWN_AS_INSTRUMENT,  // the instrument id the displayed ticker was resolved from
} shown_as_e;

/**
 * Which displayed account the value ends up with.
 *
 * Not always the one labelled as the receiver. Sending money, and accepting an offer of it, moves
 * the value to the receiver. Rejecting or withdrawing an offer returns the locked funds to the
 * sender, so the sender is the destination there.
 */
typedef enum {
    DEST_NONE = 0,  // the action moves nothing
    DEST_SENDER,
    DEST_RECEIVER,
} destination_e;

/**
 * Whether the amount shown can be compared against the destination's holding.
 *
 * Returning a locked holding hands back a fee reserve along with it, so its amount is legitimately
 * larger than the amount on screen and only the destination is checkable.
 */
typedef enum {
    AMOUNT_NOT_COMPARABLE = 0,
    AMOUNT_COMPARABLE,
} amount_check_e;

typedef struct {
    const char *path;
    const char *item_name;
    field_format_callback_t format_callback;
    bool mandatory;
    shown_as_e shown_as;
} field_config_t;

struct tx_field_t {
    char *value;
    size_t value_len;
    const field_config_t *config;
    bool found;
    bool display;
    bool store_failed;  // no room to keep its value, or the path matched twice
};

typedef struct {
    // The record that carries the displayed fields. For an exercise it is the choice argument, for
    // a create it is the template itself.
    identifier_config_t identifier;
    // The templates the node may act on. A record may only pick this screen when the node's own
    // template_id is one of them, so the screen is tied to a contract type the app supports and
    // not only to a record name the host wrote. It is a list because the same choice is performed
    // on a different template per token family. Empty for a create, whose template is the
    // identifier above.
    const identifier_config_t *const *node_identifiers;
    const size_t node_identifiers_count;
    const field_config_t *fields;
    size_t fields_count;
    const char *review_title;
    const char *review_finish;
    const identifier_config_t *const *metadata_contract_identifiers;
    const size_t metadata_contract_identifiers_count;
    // What this action does with the value, which the field paths alone cannot say.
    destination_e destination;
    amount_check_e amount_check;
} display_config_t;

struct pb_callback_context_t {
    char *field_path;
    transaction_ctx_t *tx_info;
    tx_field_t *tx_fields;
    uint8_t nb_fields;
    const char *review_title;
    const char *review_finish;
    bool unknown_token;
    char *last_parsed_contract_id;
    // The action this node signs, read from the node before its argument is walked. Only a record
    // that names this action may choose the screen, so a record nested inside the argument cannot.
    char *node_module;     // the node's own template_id, both node kinds
    char *node_entity;     //
    char *node_choice_id;  // the choice an exercise performs, NULL for a create
    // How many records deep the argument walk currently is. The screen is chosen by the outermost
    // record only, which is depth 1.
    uint8_t record_depth;
    // Copied from the matched configuration, so the value check can be handed the displayed values
    // once they are resolved.
    destination_e destination;
    amount_check_e amount_check;
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
