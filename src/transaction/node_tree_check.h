#pragma once

#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <stdint.h>   // int32_t

#include "utils.h"  // MUST_CHECK

/**
 * Result of the node tree check.
 *
 * The codes are split in two classes. Use tree_error_is_impossible() to tell them apart.
 * - An impossible tree cannot come from a correct host. Refuse the transaction.
 * - An unexpected tree may come from a future protocol version. Fall back to blind signing.
 */
typedef enum {
    TREE_OK = 0,

    /* Impossible trees */
    TREE_ERR_NODE_ID_OUT_OF_RANGE,  /// a node id is missing, not a number, or above nodes_count
    TREE_ERR_DUPLICATE_NODE,        /// the same node id arrived twice
    TREE_ERR_CHILD_OUT_OF_RANGE,    /// a child id is not a number, or above nodes_count
    TREE_ERR_SELF_CHILD,            /// a node claims itself as its own child
    TREE_ERR_ROOT_CLAIMED,          /// a node claims the root as its child
    TREE_ERR_DUPLICATE_CLAIM,       /// two parents claim the same child

    /* Unexpected trees */
    TREE_ERR_ROOT_COUNT,      /// the transaction does not have exactly one root
    TREE_ERR_ORPHAN_NODE,     /// a node arrived that no parent claimed
    TREE_ERR_MISSING_CHILD,   /// a claimed child never arrived
    TREE_ERR_TOO_MANY_NODES,  /// the node count is negative or above MAX_TX_NODES
    TREE_ERR_NO_MEMORY        /// the tracking array could not be allocated
} tree_error_e;

/**
 * @brief Start a new tree check.
 *
 * Clears the tracking state and the cached error, then records the single root. Call this once per
 * transaction, when the transaction header has been parsed.
 *
 * @param[in] roots        Root node ids from the transaction header.
 * @param[in] roots_count  Number of root node ids.
 * @param[in] nodes_count  Number of nodes the header announces.
 *
 * @return true if the tree can still be clear signed, false if an error was already cached.
 */
MUST_CHECK bool tree_check_init(char **roots, size_t roots_count, int32_t nodes_count);

/**
 * @brief Record that a node arrived.
 *
 * Caches an error if the id is out of range or if the same id already arrived.
 *
 * @param[in] node_id  Id of the node being parsed, or -1 when the node carried no id.
 */
void tree_mark_seen(int32_t node_id);

/**
 * @brief Record the children that a node declares.
 *
 * Caches an error on the first child that cannot be part of a valid tree, and stops there.
 *
 * @param[in] parent_id       Id of the node that declares the children.
 * @param[in] children        Child node ids.
 * @param[in] children_count  Number of child node ids.
 */
void tree_claim_children(int32_t parent_id, char **children, size_t children_count);

/**
 * @brief Check that every node belongs to the tree.
 *
 * Call this once, when every node has been received. Compares the nodes that arrived against the
 * nodes that a parent claimed, then returns the cached error.
 *
 * @return TREE_OK, or the first error that was cached.
 */
MUST_CHECK int tree_verify_complete(void);

/**
 * @brief Tell whether the tree is still good enough to clear sign.
 *
 * False as soon as any error is cached, so the caller can stop parsing nodes for display. Most
 * errors are known while the nodes arrive, because a node records its children before its display
 * parse runs. The orphan and missing child cases are only known once every node has arrived, so
 * they are reported by tree_verify_complete() instead.
 *
 * @return true while no error has been found.
 */
MUST_CHECK bool tree_can_clear_sign(void);

/**
 * @brief Tell whether an error means the tree could never be valid.
 *
 * @param[in] err  A tree_error_e value.
 *
 * @return true if the transaction must be refused, false if blind signing is enough.
 */
MUST_CHECK bool tree_error_is_impossible(int err);

/**
 * @brief Clear the tracking state and the cached error.
 *
 * Called from clean_context() so that no state survives into the next transaction.
 */
void tree_check_reset(void);
