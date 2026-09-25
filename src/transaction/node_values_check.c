#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <stdint.h>   // uint*_t
#include <string.h>   // memcmp, memmove, memset, strlen

#include "cx.h"  // cx_hash_sha256
#include "ledger_assert.h"
#include "os.h"  // PRINTF

#include "constants.h"
#include "node_values_check.h"
#include "utils.h"

/* -------------------------------------------------------------------------- */
/*  The store: what we saw                                                    */
/* -------------------------------------------------------------------------- */

// Values are kept as digests. A party identifier runs past 70 bytes and the parser already peaks
// near the 3 KB heap limit, so the store has to stay small and static.
typedef struct {
    uint8_t owner[SHA256_HASH_LEN];
    uint8_t amount[SHA256_HASH_LEN];
    uint8_t admin[SHA256_HASH_LEN];
} holding_t;

static struct {
    holding_t holdings[MAX_HOLDINGS];
    uint8_t holdings_count;

    // Contracts the nodes acted on, so a disclosed contract that no node used can be spotted.
    // On the heap, not in static memory: static state comes out of the stack, and Nano X has none
    // to spare. Freed once the disclosed contracts have been checked against it.
    uint8_t (*used_contracts)[USED_CONTRACT_LEN];
    uint8_t used_contracts_count;

    uint8_t destination[SHA256_HASH_LEN];  // account the value ends up with
    uint8_t amount[SHA256_HASH_LEN];       // amount shown, before formatting
    uint8_t admin[SHA256_HASH_LEN];        // instrument admin the ticker was resolved from
    // The account the value leaves, kept whichever way round the screen reads. Only used to spot a
    // transfer to oneself, where the sender and the destination are the same account.
    uint8_t sender[SHA256_HASH_LEN];
    bool has_destination, has_amount, has_admin, has_sender;
    bool bound;                       // a display configuration matched
    destination_e bound_destination;  // which account that configuration says the value goes to

    bool ok;  // false means clear signing is off, the transaction is never refused
} store;

// Clear signing stops. The transaction still goes through as a blind signature, because an extra
// holding or an unfamiliar shape is more likely a protocol change than an attack.
static void give_up(const char *reason) {
    UNUSED(reason);  // PRINTF compiles away in release, leaving reason unread

    if (store.ok) {
        PRINTF("Node values check gave up: %s\n", reason);
        store.ok = false;
    }
}

static void digest(const char *text, uint8_t out[SHA256_HASH_LEN]) {
    cx_hash_sha256((const uint8_t *) text, strlen(text), out, SHA256_HASH_LEN);
}

// Give the contract list back. Called once the disclosed contracts have been checked against it,
// and again on reset so an abandoned transaction leaves nothing behind.
void values_release_used_contracts(void) {
    if (store.used_contracts != NULL) {
        app_mem_free(store.used_contracts);
        store.used_contracts = NULL;
    }
    store.used_contracts_count = 0;
}

void values_check_reset(void) {
    values_release_used_contracts();
    memset(&store, 0, sizeof(store));
    store.ok = true;
}

// Digest of one contract identifier, truncated to USED_CONTRACT_LEN.
static void contract_digest(const char *contract_id, uint8_t out[USED_CONTRACT_LEN]) {
    uint8_t full[SHA256_HASH_LEN] = {0};
    cx_hash_sha256((const uint8_t *) contract_id, strlen(contract_id), full, sizeof(full));
    memmove(out, full, USED_CONTRACT_LEN);
}

// Remember that a node acted on this contract. Deduplicated, because a node is decoded more than
// once and the same contract is often fetched and then exercised.
void values_record_used_contract(const char *contract_id) {
    if (contract_id == NULL || !store.ok) {
        return;
    }
    if (store.used_contracts == NULL &&
        !app_mem_calloc((void **) &store.used_contracts, MAX_USED_CONTRACTS * USED_CONTRACT_LEN)) {
        give_up("no memory for the contract list");
        return;
    }

    uint8_t digest[USED_CONTRACT_LEN] = {0};
    contract_digest(contract_id, digest);

    for (uint8_t i = 0; i < store.used_contracts_count; i++) {
        if (memcmp(store.used_contracts[i], digest, USED_CONTRACT_LEN) == 0) {
            return;
        }
    }
    if (store.used_contracts_count >= MAX_USED_CONTRACTS) {
        give_up("too many contracts in one transaction");
        return;
    }
    memmove(store.used_contracts[store.used_contracts_count], digest, USED_CONTRACT_LEN);
    store.used_contracts_count++;
}

// Every contract disclosed in the metadata must have been used by a node. The nodes all arrive
// before the metadata, so the list is complete by the time this runs.
void values_check_disclosed_contract(const char *contract_id) {
    if (contract_id == NULL || !store.ok) {
        return;
    }

    uint8_t digest[USED_CONTRACT_LEN] = {0};
    contract_digest(contract_id, digest);

    for (uint8_t i = 0; i < store.used_contracts_count; i++) {
        if (memcmp(store.used_contracts[i], digest, USED_CONTRACT_LEN) == 0) {
            return;
        }
    }
    give_up("a disclosed contract was used by no node");
}

MUST_CHECK bool values_still_collecting(void) {
    return store.ok;
}

// Every holding is recorded, repeats included. Two creates can legitimately write the same owner,
// amount and admin, and the entries carry nothing that tells those two apart, so collapsing them
// would hide what the second one pays. A create is decoded once per node part, so a repeat can only
// come from a payload that writes the same value twice.
void values_report_holding(const uint8_t owner[SHA256_HASH_LEN],
                           const uint8_t amount[SHA256_HASH_LEN],
                           const uint8_t admin[SHA256_HASH_LEN]) {
    holding_t *holding = NULL;

    LEDGER_ASSERT(owner != NULL, "NULL owner in values_report_holding");
    LEDGER_ASSERT(amount != NULL, "NULL amount in values_report_holding");
    LEDGER_ASSERT(admin != NULL, "NULL admin in values_report_holding");

    if (store.holdings_count >= MAX_HOLDINGS) {
        give_up("too many holdings written");
        return;
    }

    holding = &store.holdings[store.holdings_count];
    memmove(holding->owner, owner, SHA256_HASH_LEN);
    memmove(holding->amount, amount, SHA256_HASH_LEN);
    memmove(holding->admin, admin, SHA256_HASH_LEN);
    store.holdings_count++;
}

void values_report_unknown_template(void) {
    give_up("a create node carries an unknown template");
}

void values_report_unreadable_holding(void) {
    give_up("a holding's fields could not be located");
}

void values_report_parse_failure(void) {
    give_up("a node could not be parsed for its value");
}

/* -------------------------------------------------------------------------- */
/*  The verdict: compare it against the screen                                */
/* -------------------------------------------------------------------------- */

void values_bind_from_display(const tx_field_t *fields,
                              uint8_t count,
                              destination_e destination,
                              amount_check_e amount_check) {
    const char *shown_destination = NULL;
    const char *shown_amount = NULL;
    const char *shown_admin = NULL;
    const char *shown_sender = NULL;

    LEDGER_ASSERT(fields != NULL, "NULL fields in values_bind_from_display");

    // A second screen can bind in the same transaction, so drop the first screen's values. Without
    // this the checks below would compare the new screen against the values the old one left.
    store.has_destination = false;
    store.has_amount = false;
    store.has_admin = false;
    store.has_sender = false;

    for (uint8_t i = 0; i < count; i++) {
        const tx_field_t *field = &fields[i];

        if (!field->found || field->value == NULL || field->config == NULL) {
            continue;
        }
        switch (field->config->shown_as) {
            case SHOWN_AS_SENDER:
                // Kept even when the destination is the receiver, so a transfer to oneself can be
                // told apart from a transfer to somebody else.
                shown_sender = field->value;
                if (destination == DEST_SENDER) {
                    shown_destination = field->value;
                }
                break;
            case SHOWN_AS_RECEIVER:
                if (destination == DEST_RECEIVER) {
                    shown_destination = field->value;
                }
                break;
            case SHOWN_AS_AMOUNT:
                if (amount_check == AMOUNT_COMPARABLE) {
                    shown_amount = field->value;
                }
                break;
            case SHOWN_AS_ADMIN:
                shown_admin = field->value;
                break;
            default:
                break;
        }
    }

    if (shown_destination != NULL) {
        digest(shown_destination, store.destination);
        store.has_destination = true;
    }
    if (shown_amount != NULL) {
        digest(shown_amount, store.amount);
        store.has_amount = true;
    }
    if (shown_admin != NULL) {
        digest(shown_admin, store.admin);
        store.has_admin = true;
    }
    if (shown_sender != NULL) {
        digest(shown_sender, store.sender);
        store.has_sender = true;
    }
    store.bound = true;
    store.bound_destination = destination;
}

// Exactly one holding may name the account the value ends up with, and it must carry the amount
// shown whenever that amount is comparable. Requiring one to exist matters on its own: without it,
// a payload naming a destination that receives nothing would pass unnoticed. Requiring no more than
// one is what bounds the payment: the amounts are digests and cannot be added up, so a second
// holding for that account could pay it anything on top of the amount on screen.
//
// Every recorded transfer writes one holding for its destination. The change and the escrow go to
// the sender, who is not the destination on those screens, and on the reject and withdraw screens,
// where the sender is the destination, the returned lock is the only holding written.
// The screen shows the value leaving and arriving at the same account, which is what consolidating
// your own holdings looks like: the transfer machinery is used to merge several into fewer.
//
// Only a screen that names a receiver can say this. On the reject and withdraw screens the sender
// is also the destination, so the two are the same field and would always compare equal.
MUST_CHECK static bool is_transfer_to_self(void) {
    return store.bound_destination == DEST_RECEIVER && store.has_sender && store.has_destination &&
           memcmp(store.sender, store.destination, SHA256_HASH_LEN) == 0;
}

MUST_CHECK static bool destination_holds_value(void) {
    uint8_t owned = 0;
    bool amount_seen = false;

    for (uint8_t i = 0; i < store.holdings_count; i++) {
        if (memcmp(store.holdings[i].owner, store.destination, SHA256_HASH_LEN) != 0) {
            // Change and escrow go back to the sender. Any other owner is an account the screen
            // never names, receiving value the user was never shown.
            if (!store.has_sender ||
                memcmp(store.holdings[i].owner, store.sender, SHA256_HASH_LEN) != 0) {
                give_up("a holding goes to an account the screen does not show");
                return false;
            }
            continue;
        }
        owned++;
        // Returning a locked holding hands back a fee reserve with it, so no amount was bound and
        // the destination owning a holding is all there is to check.
        if (!store.has_amount ||
            memcmp(store.holdings[i].amount, store.amount, SHA256_HASH_LEN) == 0) {
            amount_seen = true;
        }
    }

    if (owned == 0) {
        give_up("no holding for the account the value goes to");
        return false;
    }

    // A second holding for the destination is normally value the screen never showed. Sending to
    // yourself is the exception: the amount moved and the change both land on the one account, and
    // no value leaves, so counting them proves nothing.
    if (owned > 1 && !is_transfer_to_self()) {
        give_up("more than one holding for the account the value goes to");
        return false;
    }

    if (!amount_seen) {
        give_up("the destination's holding has another amount");
        return false;
    }

    return true;
}

// Every holding must be issued by the party the displayed ticker was resolved from. The ticker
// comes from the instrument admin of the choice argument, and nothing otherwise ties that to the
// holdings actually moving, so a payload could name one token on screen while moving another.
MUST_CHECK static bool holdings_match_displayed_instrument(void) {
    for (uint8_t i = 0; i < store.holdings_count; i++) {
        if (memcmp(store.holdings[i].admin, store.admin, SHA256_HASH_LEN) != 0) {
            give_up("a holding was issued by another party");
            return false;
        }
    }
    return true;
}

MUST_CHECK bool values_can_clear_sign(void) {
    if (!store.ok) {
        return false;
    }

    // Nothing matched, so there is no screen to compare the values against.
    if (!store.bound) {
        return false;
    }

    // A screen with no destination authorizes something without moving value, so the transaction
    // must write no holding at all. The two checks below need a destination to compare against, so
    // skipping them would let the payload move anything it likes behind such a screen.
    if (store.bound_destination == DEST_NONE) {
        if (store.holdings_count > 0) {
            give_up("a screen that moves nothing wrote a holding");
            return false;
        }
        return true;
    }

    // The screen names a destination, so it must have resolved. Every configuration that names one
    // marks that field mandatory, so this only fires if the table and this check disagree.
    if (!store.has_destination) {
        give_up("the screen's destination field did not resolve");
        return false;
    }

    if (!destination_holds_value()) {
        return false;
    }

    if (store.has_admin && !holdings_match_displayed_instrument()) {
        return false;
    }

    return true;
}
