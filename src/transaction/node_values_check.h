#pragma once

#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <stdint.h>   // uint8_t

#include "constants.h"  // SHA256_HASH_LEN

#include "pb_node_display_definitions.h"  // tx_field_t, destination_e, amount_check_e
#include "utils.h"                        // MUST_CHECK

/**
 * Value check for a prepared transaction.
 *
 * The node tree check proves the payload is one tree. This check looks at what the tree moves.
 * Canton keeps no balance: each amount owned is its own small contract with an owner and an amount
 * written in it, like a note in a wallet. The Token Standard calls such a contract a holding.
 * pb_holding_parser.h finds them; this decides what they mean.
 *
 * The checks are:
 *  - Exactly one holding must name the account the value ends up with, and it must carry the amount
 *    shown whenever that amount is comparable. One and no more, because the amounts are digests and
 *    cannot be added up: a second holding for that account would pay it more than the screen says.
 *  - Every holding must be issued by the same party the displayed ticker was resolved from.
 *  - A create whose template is unknown drops clear signing, because nothing classified it.
 *
 * The account the value ends up with is not always the one the screen labels as the receiver.
 * Sending money, and accepting an offer of it, moves the value to the receiver. Rejecting or
 * withdrawing an offer returns the locked funds to the sender, so the sender is the destination
 * there, and the returned lock still holds a fee reserve, which makes its amount legitimately
 * larger than the amount on screen. The display configuration says which is which, and for those
 * two only the destination is checked.
 *
 * A screen that names no destination is held to a stricter rule instead of a weaker one. The
 * pre-approval proposal authorizes future transfers without moving anything, so there is nothing to
 * compare a holding against; a transaction behind such a screen must therefore write no holding at
 * all. Were the checks merely skipped, one transaction could show a pre-approval screen and still
 * carry creates that pay an attacker.
 *
 * There is deliberately no check that the account shown as the sender is this device's own account.
 * A stolen signature is only worth something when this device's party is an authorizing party, so a
 * real attack carries the correct sender and lies about the amount or the destination instead. A
 * transaction with a foreign sender produces a signature the ledger rejects on its own. The checks
 * above hold the actual protection.
 *
 * The contracts a consuming exercise destroys are no longer recorded. That list was the join key
 * for bounding the sender's total outflow, which needs the fee question answered first, and until
 * then nothing read it. On this device static state is taken out of the stack, so a list nobody
 * reads is not free: it cost 160 bytes that the deepest input-contract parse needs. It can come
 * back when the outflow check does.
 *
 * Nothing here refuses a transaction. Every failure only drops clear signing, because an extra
 * holding or an unfamiliar shape is more likely a protocol change than an attack.
 *
 * Do not add a check that every account appearing in the transaction must be one shown on screen.
 * A normal transfer pays a reward to a validator that the screen never names, so that would drop
 * every transfer to blind signing.
 */

/**
 * @brief Clear the collected values.
 *
 * Called from clean_context() so nothing survives into the next transaction.
 */
void values_check_reset(void);

/**
 * @brief Tell whether collecting is still worth the work.
 *
 * @return false once clear signing has been dropped, so the ctx can stop early.
 */
MUST_CHECK bool values_still_collecting(void);

/**
 * @brief Record one holding a create node writes.
 *
 * Takes digests, not strings. A party identifier runs to about 115 bytes, and holding three of
 * those in static state cost the stack more than this device can spare, so the parser hashes each
 * value as it captures it. Comparison is unaffected: the displayed values are hashed too, so a
 * digest match still means the strings matched byte for byte.
 *
 * Repeats are kept, not collapsed. Two creates can write the same owner, amount and admin, and
 * nothing recorded here tells them apart, so dropping the second one would hide what it pays.
 *
 * @param[in] owner   Digest of the account the holding belongs to.
 * @param[in] amount  Digest of the amount written in it, as the ledger wrote it.
 * @param[in] admin       Digest of the party that issued it.
 * @param[in] instrument  Digest of the instrument id it holds, or NULL when its template names
 * none.
 */
void values_report_holding(const uint8_t owner[SHA256_HASH_LEN],
                           const uint8_t amount[SHA256_HASH_LEN],
                           const uint8_t admin[SHA256_HASH_LEN],
                           const uint8_t *instrument);

/**
 * @brief Report a create node whose template no list classifies.
 *
 * Nothing paired an owner with an amount for it, so what it creates is unknown and clear signing
 * stops.
 */
void values_report_unknown_template(void);

/**
 * @brief Report a listed template whose configured field paths did not resolve.
 *
 * The table no longer describes the contract, so what it holds is unknown.
 */
void values_report_unreadable_holding(void);

/**
 * @brief Report that a node could not be decoded for its value.
 */
void values_report_parse_failure(void);

/**
 * @brief Give the value check what the review screen shows.
 *
 * Called when a display configuration has matched and its field values are resolved. Each field
 * says what it is shown as, and the two enums say what this action does with the value, so the same
 * field list can serve both accept and reject.
 *
 * A second screen can bind in the same transaction, because dropping clear signing over an unknown
 * token lets parsing carry on to the next node. Each call therefore replaces the previous screen's
 * values rather than adding to them.
 *
 * Must run before the formatting callbacks. A holding carries the amount exactly as the ledger
 * wrote it, "20.0000000000", while the screen shows "20 CC", so only the unformatted string
 * compares byte for byte against a holding.
 *
 * @param[in] fields        The matched configuration's resolved fields.
 * @param[in] count         How many of them there are.
 * @param[in] destination   Which displayed account the value ends up with.
 * @param[in] amount_check  Whether the displayed amount can be compared against a holding.
 */
void values_bind_from_display(const tx_field_t *fields,
                              uint8_t count,
                              destination_e destination,
                              amount_check_e amount_check);

/**
 * @brief Tell whether the collected values agree with the review screen.
 *
 * @return true when every check that applies passes.
 */
MUST_CHECK bool values_can_clear_sign(void);

/**
 * @brief Remember that a node acted on this contract.
 *
 * Deduplicated. Kept so that values_check_disclosed_contract() can tell whether a contract
 * disclosed in the metadata was actually used.
 *
 * @param[in] contract_id  Contract the node acted on.
 */
void values_record_used_contract(const char *contract_id);

/**
 * @brief Check one contract disclosed in the metadata against the contracts the nodes used.
 *
 * Gives up on clear signing when no node used it. The nodes all arrive before the metadata, so the
 * list is complete when this runs.
 *
 * @param[in] contract_id  Contract disclosed in the metadata.
 */
void values_check_disclosed_contract(const char *contract_id);

/**
 * @brief Free the contract list, once the disclosed contracts have been checked against it.
 */
void values_release_used_contracts(void);
