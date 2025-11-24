#include "pb_node_display_definitions.h"
#include <stddef.h>

/* -------------------------------------------------------------------------- */
/* Macros for Concise Configuration                                           */
/* -------------------------------------------------------------------------- */

// Use distinct argument names (_p, _n) to avoid any potential macro expansion conflicts
#define FLD(_p, _n, _f, _m) \
    { .path = _p, .item_name = _n, .format_callback = _f, .mandatory = _m }
#define ID(_mod, _ent) \
    { .module_name = _mod, .entity_name = _ent }

/* -------------------------------------------------------------------------- */
/* Global Constants / Mappings                                                */
/* -------------------------------------------------------------------------- */

const char *const INSTRUMENT_ID_TO_TICKER_MAPPING[] = {
    "Amulet",
    NATIVE_COIN_TICKER,
    "amulet",
    NATIVE_COIN_TICKER,
};
const size_t INSTRUMENT_ID_TO_TICKER_MAPPING_NB =
    sizeof(INSTRUMENT_ID_TO_TICKER_MAPPING) / (2 * sizeof(char *));

/* -------------------------------------------------------------------------- */
/* Reusable Field Definitions (Static arrays)                                 */
/* -------------------------------------------------------------------------- */

const field_config_t INSTRUMENT_ID_FIELD = FLD("transfer.instrumentId.id", "Token", NULL, true);
const field_config_t PROXY_INSTRUMENT_ID_FIELD =
    FLD("proxyArg.choiceArg.transfer.instrumentId.id", "Token", NULL, true);
const field_config_t PREAPPROVAL_ASSET_FIELD = FLD("asset", "For asset", NULL, true);
static const identifier_config_t META_ID =
    ID("Splice.AmuletTransferInstruction", "AmuletTransferInstruction");

/* -------------------------------------------------------------------------- */
/* Field Lists                                                                */
/* -------------------------------------------------------------------------- */

static const field_config_t TOKEN_TRANSFER_FIELDS[] = {
    FLD("transfer.sender", "From", NULL, true),
    FLD("transfer.amount", "Amount", format_token_amount_field, true),
    FLD("transfer.receiver", "To", NULL, true),
    FLD("transfer.instrumentId.id", "Token", NULL, true),
    FLD("transfer.meta.values.splice\\.lfdecentralizedtrust\\.org/reason", "Memo", NULL, false)};

static const field_config_t TOKEN_TRANSFER_ACCEPT_FIELDS[] = {
    FLD("transfer.sender", "From", NULL, true),
    FLD("transfer.amount", "Amount", format_token_amount_field, true),
    FLD("transfer.receiver", "To", NULL, true),
    FLD("transfer.instrumentId.id", "Token", NULL, true),
    FLD("transfer.executeBefore", "Expiration time", format_timestamp_field, true),
    FLD("transfer.meta.values.splice\\.lfdecentralizedtrust\\.org/reason", "Memo", NULL, false)};

static const field_config_t TOKEN_TRANSFER_WITHDRAW_FIELDS[] = {
    FLD("transfer.sender", "Withdraw to", format_token_amount_field, true),
    FLD("transfer.amount", "Amount", format_token_amount_field, true),
    FLD("transfer.instrumentId.id", "Token", NULL, true)};

static const field_config_t NATIVE_COIN_TRANSFER_FIELDS[] = {
    FLD("sender", "From", NULL, true),
    FLD("amount", "Amount", format_native_amount_field, true),
    FLD("receiver", "To", NULL, true),
    FLD("description", "Memo", NULL, false)};

static const field_config_t PREAPPROVAL_PROPOSAL_FIELDS[] = {
    FLD("receiver", "Pre-approve for account", NULL, true),
    FLD("asset", "For asset", NULL, true),
    FLD("provider", "By validator", NULL, true)};

static const field_config_t PROXY_TRANSFER_FIELDS[] = {
    FLD("proxyArg.choiceArg.transfer.sender", "From", NULL, true),
    FLD("proxyArg.choiceArg.transfer.amount", "Amount", format_token_amount_field, true),
    FLD("proxyArg.choiceArg.transfer.receiver", "To", NULL, true),
    FLD("proxyArg.choiceArg.transfer.instrumentId.id", "Token", NULL, true),
    FLD("proxyArg.choiceArg.transfer.meta.values.splice\\.lfdecentralizedtrust\\.org/reason",
        "Memo",
        NULL,
        false)};

/* -------------------------------------------------------------------------- */
/* Main Display Configuration                                                 */
/* -------------------------------------------------------------------------- */

#define CFG_ENTRY(_id, _meta, _fields, _title, _finish)                               \
    {                                                                                 \
        .identifier = _id, .metadata_contract_identifier = _meta, .fields = _fields,  \
        .fields_count = sizeof(_fields) / sizeof(_fields[0]), .review_title = _title, \
        .review_finish = _finish                                                      \
    }

const display_config_t DISPLAY_CONFIGS[] = {
    CFG_ENTRY(ID("Splice.Api.Token.TransferInstructionV1", "TransferFactory_Transfer"),
              NULL,
              TOKEN_TRANSFER_FIELDS,
              TOKEN_TRANSFER_REVIEW_TITLE,
              TOKEN_TRANSFER_REVIEW_FINISH),

    CFG_ENTRY(
        ID("Splice.ExternalPartyAmuletRules", "ExternalPartyAmuletRules_CreateTransferCommand"),
        NULL,
        NATIVE_COIN_TRANSFER_FIELDS,
        NATIVE_COIN_TRANSFER_REVIEW_TITLE,
        NATIVE_COIN_TRANSFER_REVIEW_FINISH),

    CFG_ENTRY(ID("Splice.Wallet.TransferPreapproval", "TransferPreapprovalProposal"),
              NULL,
              PREAPPROVAL_PROPOSAL_FIELDS,
              PREAPPROVAL_PROPOSAL_REVIEW_TITLE,
              PREAPPROVAL_PROPOSAL_REVIEW_FINISH),

    CFG_ENTRY(
        ID("Splice.Util.FeaturedApp.WalletUserProxy", "WalletUserProxy_TransferFactory_Transfer"),
        NULL,
        PROXY_TRANSFER_FIELDS,
        TOKEN_TRANSFER_REVIEW_TITLE,
        TOKEN_TRANSFER_REVIEW_FINISH),

    CFG_ENTRY(ID("Splice.Api.Token.TransferInstructionV1", "TransferInstruction_Accept"),
              &META_ID,
              TOKEN_TRANSFER_ACCEPT_FIELDS,
              TOKEN_TRANSFER_ACCEPT_REVIEW_TITLE,
              TOKEN_TRANSFER_ACCEPT_REVIEW_FINISH),

    CFG_ENTRY(ID("Splice.Api.Token.TransferInstructionV1", "TransferInstruction_Reject"),
              &META_ID,
              TOKEN_TRANSFER_ACCEPT_FIELDS,
              TOKEN_TRANSFER_REJECT_REVIEW_TITLE,
              TOKEN_TRANSFER_REJECT_REVIEW_FINISH),

    CFG_ENTRY(ID("Splice.Api.Token.TransferInstructionV1", "TransferInstruction_Withdraw"),
              &META_ID,
              TOKEN_TRANSFER_WITHDRAW_FIELDS,
              TOKEN_TRANSFER_WITHDRAW_REVIEW_TITLE,
              TOKEN_TRANSFER_WITHDRAW_REVIEW_FINISH),
};

const size_t DISPLAY_CONFIGS_NB = sizeof(DISPLAY_CONFIGS) / sizeof(DISPLAY_CONFIGS[0]);
