#include "pb_node_display_definitions.h"
#include <stddef.h>

/* -------------------------------------------------------------------------- */
/* Macros for Concise Configuration                                           */
/* -------------------------------------------------------------------------- */

// Use distinct argument names (_p, _n) to avoid any potential macro expansion conflicts
#define FLD(_p, _n, _f, _m) {.path = _p, .item_name = _n, .format_callback = _f, .mandatory = _m}
#define ID(_mod, _ent)      {.module_name = _mod, .entity_name = _ent}

/* -------------------------------------------------------------------------- */
/* Global Constants / Mappings                                                */
/* -------------------------------------------------------------------------- */

const instrument_to_ticker_mapping_t INSTRUMENT_TO_TICKER_MAPPINGS[] = {
    {
        .admin = "DSO::"
                 "1220be58c29e65de40bf273be1dc2b266d43a9a002ea5b"
                 "18955aeef7aac881bb471a",  // Devnet
        .id = "Amulet",                     // Uppercase
        .ticker = NATIVE_COIN_TICKER        // CC
    },
    {
        .admin = "DSO::"
                 "1220be58c29e65de40bf273be1dc2b266d43a9a002ea5b"
                 "18955aeef7aac881bb471a",  // Devnet
        .id = "amulet",                     // lowercase
        .ticker = NATIVE_COIN_TICKER        // CC
    },
    {.admin = "cbtc-network::"
              "12202a83c6f4082217c175e29bc53da5f2703ba2675778ab9"
              "9217a5a881a949203ff",  // Devnet
     .id = "CBTC",
     .ticker = "CBTC"},
    {
        .admin = "DSO::"
                 "1220f22a8b8f2d813c25b9a684dc4dd52b532a0174d8e7"
                 "3a13cdf2baabfff7518337",  // Testnet
        .id = "Amulet",                     // Uppercase
        .ticker = NATIVE_COIN_TICKER        // CC
    },
    {
        .admin = "DSO::"
                 "1220f22a8b8f2d813c25b9a684dc4dd52b532a0174d8e7"
                 "3a13cdf2baabfff7518337",  // Testnet
        .id = "amulet",                     // lowercase
        .ticker = NATIVE_COIN_TICKER        // CC
    },
    {.admin = "cbtc-network::"
              "12201b1741b63e2494e4214cf0bedc3d5a224da53b3bf4d76"
              "dba468f8e97eb15508f",  // Testnet
     .id = "CBTC",
     .ticker = "CBTC"},
    {
        .admin = "DSO::"
                 "1220b1431ef217342db44d516bb9befde802be7d889963"
                 "7d290895fa58880f19accc",  // Mainnet
        .id = "Amulet",                     // Uppercase
        .ticker = NATIVE_COIN_TICKER        // CC
    },
    {
        .admin = "DSO::"
                 "1220b1431ef217342db44d516bb9befde802be7d889963"
                 "7d290895fa58880f19accc",  // Mainnet
        .id = "amulet",                     // lowercase
        .ticker = NATIVE_COIN_TICKER        // CC
    },
    {.admin = "cbtc-network::"
              "12205af3b949a04776fc48cdcc05a060f6bda2e470632935f"
              "375d1049a8546a3b262",  // Mainnet
     .id = "CBTC",
     .ticker = "CBTC"},
    {.admin = "party-28dc4516-b5ca-44ff-86c7-2107e90a6807::"
              "1220b8301e18aa8a401d6e34e6c20f8b0243183c514373bca8f1b6b9270246341a9e",  // Mainnet
     .id = "f29bdd7a-1469-498a-ba2a-796bf5387b31",
     .ticker = "SBC"},
    {.admin = "decentralized-usdc-interchain-rep::"
              "122049e2af8a725bd19759320fc83c638e7718973eac189d8f201309c512d1ffec61",  // Testnet
     .id = "USDCx",
     .ticker = "USDCx"},
    {.admin = "decentralized-usdc-interchain-rep::"
              "12208115f1e168dd7e792320be9c4ca720c751a02a3053c7606e1c1cd3dad9bf60ef",  // Mainnet
     .id = "USDCx",
     .ticker = "USDCx"},
    {.admin = "party-28dc4516-b5ca-44ff-86c7-2107e90a6807::"
              "1220b8301e18aa8a401d6e34e6c20f8b0243183c514373bca8f1b6b9270246341a9e",  // Mainnet
     .id = "481871d4-ca56-42a8-b2d3-4b7d28742946",
     .ticker = "CUSD"}};

const size_t INSTRUMENT_TO_TICKER_MAPPING_NB =
    sizeof(INSTRUMENT_TO_TICKER_MAPPINGS) / sizeof(instrument_to_ticker_mapping_t);

/* -------------------------------------------------------------------------- */
/* Reusable Field Definitions (Static arrays)                                 */
/* -------------------------------------------------------------------------- */

const field_config_t INSTRUMENT_ID_FIELD = FLD("transfer.instrumentId.id", "Token", NULL, true);
const field_config_t PROXY_INSTRUMENT_ID_FIELD =
    FLD("proxyArg.choiceArg.transfer.instrumentId.id", "Token", NULL, true);
const field_config_t INSTRUMENT_ID_ADMIN_FIELD =
    FLD("transfer.instrumentId.admin", NULL, NULL, true);
const field_config_t INSTRUMENT_ID_PROXY_ADMIN_FIELD =
    FLD("proxyArg.choiceArg.transfer.instrumentId.admin", NULL, NULL, true);
const field_config_t PREAPPROVAL_ASSET_FIELD = FLD("asset", "For asset", NULL, true);

static const identifier_config_t META_ID_TRANSFER_INSTRUCTION =
    ID("Splice.AmuletTransferInstruction", "AmuletTransferInstruction");
static const identifier_config_t META_ID_TRANSFER_OFFER =
    ID("Utility.Registry.App.V0.Model.Transfer", "TransferOffer");

/* -------------------------------------------------------------------------- */
/* Field Lists                                                                */
/* -------------------------------------------------------------------------- */

static const field_config_t TOKEN_TRANSFER_FIELDS[] = {
    FLD("transfer.sender", "From", NULL, true),
    FLD("transfer.amount", "Amount", format_token_amount_field, true),
    FLD("transfer.receiver", "To", NULL, true),
    FLD("transfer.instrumentId.id", "Token", NULL, true),
    FLD("transfer.instrumentId.admin", NULL, NULL, true),
    FLD("transfer.meta.values.splice\\.lfdecentralizedtrust\\.org/reason", "Memo", NULL, false)};

static const field_config_t TOKEN_TRANSFER_ACCEPT_FIELDS[] = {
    FLD("transfer.sender", "From", NULL, true),
    FLD("transfer.amount", "Amount", format_token_amount_field, true),
    FLD("transfer.receiver", "To", NULL, true),
    FLD("transfer.instrumentId.id", "Token", NULL, true),
    FLD("transfer.instrumentId.admin", NULL, NULL, true),
    FLD("transfer.executeBefore", "Expiration time", format_timestamp_field, true),
    FLD("transfer.meta.values.splice\\.lfdecentralizedtrust\\.org/reason", "Memo", NULL, false)};

static const field_config_t TOKEN_TRANSFER_WITHDRAW_FIELDS[] = {
    FLD("transfer.sender", "Withdraw to", format_token_amount_field, true),
    FLD("transfer.amount", "Amount", format_token_amount_field, true),
    FLD("transfer.instrumentId.id", "Token", NULL, true),
    FLD("transfer.instrumentId.admin", NULL, NULL, true)};

static const field_config_t NATIVE_COIN_TRANSFER_FIELDS[] = {
    FLD("sender", "From", NULL, true),
    FLD("amount", "Amount", format_native_amount_field, true),
    FLD("receiver", "To", NULL, true),
    FLD("description", "Memo", NULL, false)};

static const field_config_t PREAPPROVAL_PROPOSAL_FIELDS[] = {
    FLD("receiver", "Pre-approve for account", NULL, true),
    FLD("asset", "For asset", NULL, true),
    FLD("provider", "By validator", NULL, true)};

/* -------------------------------------------------------------------------- */
/* Metadata identifiers lists                                                 */
/* -------------------------------------------------------------------------- */

static const identifier_config_t *const TRANSFER_OFFER_META_ID_LIST[] = {
    &META_ID_TRANSFER_INSTRUCTION,
    &META_ID_TRANSFER_OFFER};
static const identifier_config_t *META_EMPTY[] = {};

/* -------------------------------------------------------------------------- */
/* Main Display Configuration                                                 */
/* -------------------------------------------------------------------------- */

#define CFG_ENTRY(_id, _fields, _title, _finish, _meta)    \
    {.identifier = _id,                                    \
     .fields = _fields,                                    \
     .fields_count = sizeof(_fields) / sizeof(_fields[0]), \
     .review_title = _title,                               \
     .review_finish = _finish,                             \
     .metadata_contract_identifiers = _meta,               \
     .metadata_contract_identifiers_count = sizeof(_meta) / sizeof(_meta[0])}

const display_config_t DISPLAY_CONFIGS[] = {
    CFG_ENTRY(ID("Splice.Api.Token.TransferInstructionV1", "TransferFactory_Transfer"),
              TOKEN_TRANSFER_FIELDS,
              TOKEN_TRANSFER_REVIEW_TITLE,
              TOKEN_TRANSFER_REVIEW_FINISH,
              META_EMPTY),

    CFG_ENTRY(
        ID("Splice.ExternalPartyAmuletRules", "ExternalPartyAmuletRules_CreateTransferCommand"),
        NATIVE_COIN_TRANSFER_FIELDS,
        NATIVE_COIN_TRANSFER_REVIEW_TITLE,
        NATIVE_COIN_TRANSFER_REVIEW_FINISH,
        META_EMPTY),

    CFG_ENTRY(ID("Splice.Wallet.TransferPreapproval", "TransferPreapprovalProposal"),
              PREAPPROVAL_PROPOSAL_FIELDS,
              PREAPPROVAL_PROPOSAL_REVIEW_TITLE,
              PREAPPROVAL_PROPOSAL_REVIEW_FINISH,
              META_EMPTY),

    CFG_ENTRY(ID("Splice.Api.Token.TransferInstructionV1", "TransferInstruction_Accept"),
              TOKEN_TRANSFER_ACCEPT_FIELDS,
              TOKEN_TRANSFER_ACCEPT_REVIEW_TITLE,
              TOKEN_TRANSFER_ACCEPT_REVIEW_FINISH,
              TRANSFER_OFFER_META_ID_LIST),

    CFG_ENTRY(ID("Splice.Api.Token.TransferInstructionV1", "TransferInstruction_Reject"),
              TOKEN_TRANSFER_ACCEPT_FIELDS,
              TOKEN_TRANSFER_REJECT_REVIEW_TITLE,
              TOKEN_TRANSFER_REJECT_REVIEW_FINISH,
              TRANSFER_OFFER_META_ID_LIST),

    CFG_ENTRY(ID("Splice.Api.Token.TransferInstructionV1", "TransferInstruction_Withdraw"),
              TOKEN_TRANSFER_WITHDRAW_FIELDS,
              TOKEN_TRANSFER_WITHDRAW_REVIEW_TITLE,
              TOKEN_TRANSFER_WITHDRAW_REVIEW_FINISH,
              TRANSFER_OFFER_META_ID_LIST),

};

const size_t DISPLAY_CONFIGS_NB = sizeof(DISPLAY_CONFIGS) / sizeof(DISPLAY_CONFIGS[0]);
