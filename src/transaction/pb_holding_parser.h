#pragma once

#include "buffer.h"  // buffer_t

/**
 * Pull the value out of one transaction node.
 *
 * Canton keeps no balance: each amount owned is its own small contract with an owner and an amount
 * written in it, like a note in a wallet. The Token Standard calls such a contract a holding, and
 * the app sees one as a create node.
 *
 * Protobuf tags a leaf as a party or as a number, but it never says which party owns the contract
 * or which number is its amount, so the two cannot be paired by type. HOLDING_TEMPLATES names the
 * label path of the owner, the amount and the admin for every template that holds value, and this
 * module reads those paths.
 *
 * The ctx is deliberately dumb. It resolves two or three fixed label paths per template and
 * nothing else: no arbitrary configured paths, no escaped map-key paths, no ticker lookup and no
 * deferred matching. The display parser owns all of that and does not need to own this too.
 *
 * What it finds is handed to node_values_check.h, which decides what it means.
 */

/**
 * @brief Parse one transaction node for the value it moves.
 *
 * Reports the holding a create node writes. Decodes the buffer into its own message with its own
 * field descriptors, so the hashing and display parsers that already decoded it are left
 * untouched. Node kinds that write no holding are read past without being decoded.
 *
 * The disclosed metadata input contracts are deliberately not parsed. Nothing reads what they hold
 * yet, so the ctx would be pure cost, and contracts like AmuletRules are large enough that a
 * second decode of them presses on a heap the parser already nearly fills.
 *
 * @param[in] buf  The node part, as it arrived.
 */
void values_collect_from_node(const buffer_t *buf);
