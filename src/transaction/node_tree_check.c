#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <stdint.h>   // int32_t
#include <string.h>   // memset

#include "os.h"

#include "constants.h"
#include "mem.h"  // app_mem_alloc, app_mem_free
#include "node_tree_check.h"
#include "utils.h"

/* -------------------------------------------------------------------------- */
/*  State                                                                     */
/* -------------------------------------------------------------------------- */

typedef struct {
    bool seen;     /// the node arrived
    bool claimed;  /// a parent declared this node as its child, or it is the root
} node_check_t;

typedef struct {
    node_check_t *nodes;  /// one entry per node id, allocated for the announced node count
    int32_t root_id;      /// id of the single root, -1 when unknown
    int32_t nodes_count;  /// node count announced by the header
    tree_error_e err;     /// first error found, TREE_OK when none
} tree_check_context_t;

static tree_check_context_t check_ctx = {0};

// Keep the first error. A later error does not tell us more than the first one.
static void set_tree_error(tree_error_e err) {
    if (check_ctx.err == TREE_OK) {
        check_ctx.err = err;
        PRINTF("Node tree error: %d\n", err);
    }
}

// Node ids travel as strings. Return -1 when the id cannot be one, so the caller reports it as
// out of range. The length limit matches decode_node_id_field, which reads a node id into a
// 4 byte buffer, and it keeps atoint from overflowing on a long child id.
static int32_t node_id_from_str(const char *str) {
    if (str == NULL || str[0] == '\0' || strlen(str) > 3) {
        return -1;
    }

    // atoint returns 0 for a non-numeric string, which would read as node 0. Reject the digits
    // first so a malformed id is reported as out of range instead.
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (!is_digit(str[i])) {
            return -1;
        }
    }

    return atoint(str);
}

/* -------------------------------------------------------------------------- */
/*  Public functions                                                          */
/* -------------------------------------------------------------------------- */

void tree_check_reset(void) {
    if (check_ctx.nodes != NULL) {
        app_mem_free(check_ctx.nodes);
    }
    memset(&check_ctx, 0, sizeof(check_ctx));
    check_ctx.root_id = -1;  // zero is a valid node id, so an unknown root is -1
}

MUST_CHECK bool tree_check_init(char **roots, size_t roots_count, int32_t nodes_count) {
    tree_check_reset();

    // The cap only bounds the allocation below. A real transaction uses a handful of nodes.
    if (nodes_count < 0 || nodes_count > MAX_TX_NODES) {
        set_tree_error(TREE_ERR_TOO_MANY_NODES);
        return false;
    }

    // The app shows one action and its consequences, so it supports one root only.
    if (roots == NULL || roots_count != 1) {
        set_tree_error(TREE_ERR_ROOT_COUNT);
        return false;
    }

    // app_mem_calloc zeroes the array, and leaves it NULL when the node count is zero.
    if (!app_mem_calloc((void **) &check_ctx.nodes, nodes_count * sizeof(node_check_t))) {
        set_tree_error(TREE_ERR_NO_MEMORY);
        return false;
    }
    check_ctx.nodes_count = nodes_count;

    int32_t root_id = node_id_from_str(roots[0]);
    if (root_id < 0 || root_id >= nodes_count) {
        set_tree_error(TREE_ERR_NODE_ID_OUT_OF_RANGE);
        return false;
    }

    check_ctx.root_id = root_id;
    // The header claims the root, so the final sweep can compare seen against claimed for every
    // node without treating the root as a special case.
    check_ctx.nodes[root_id].claimed = true;

    return true;
}

// check_ctx.nodes is never NULL while check_ctx.nodes_count is above zero, because the count is set
// only after the array is allocated. So the range check below also covers the array being absent.
void tree_mark_seen(int32_t node_id) {
    if (node_id < 0 || node_id >= check_ctx.nodes_count) {
        set_tree_error(TREE_ERR_NODE_ID_OUT_OF_RANGE);
        return;
    }

    if (check_ctx.nodes[node_id].seen) {
        set_tree_error(TREE_ERR_DUPLICATE_NODE);
        return;
    }

    check_ctx.nodes[node_id].seen = true;
}

void tree_claim_children(int32_t parent_id, char **children, size_t children_count) {
    if (children == NULL) {
        return;
    }

    for (size_t i = 0; i < children_count; i++) {
        int32_t child_id = node_id_from_str(children[i]);

        if (child_id < 0 || child_id >= check_ctx.nodes_count) {
            set_tree_error(TREE_ERR_CHILD_OUT_OF_RANGE);
            return;
        }
        if (child_id == parent_id) {
            set_tree_error(TREE_ERR_SELF_CHILD);
            return;
        }
        if (child_id == check_ctx.root_id) {
            set_tree_error(TREE_ERR_ROOT_CLAIMED);
            return;
        }
        if (check_ctx.nodes[child_id].claimed) {
            set_tree_error(TREE_ERR_DUPLICATE_CLAIM);
            return;
        }

        check_ctx.nodes[child_id].claimed = true;
    }
}

MUST_CHECK int tree_verify_complete(void) {
    // A node that arrived without a parent is an extra root in disguise. A node that a parent
    // claimed but that never arrived is a hole in the tree.
    for (int32_t i = 0; i < check_ctx.nodes_count; i++) {
        if (check_ctx.nodes[i].seen && !check_ctx.nodes[i].claimed) {
            set_tree_error(TREE_ERR_ORPHAN_NODE);
            break;
        }
        // Cannot happen today, kept as a guard. The app waits for nodes_count parts, so every
        // valid id arrives. A gap always shows up as an earlier error.
        if (!check_ctx.nodes[i].seen && check_ctx.nodes[i].claimed) {
            set_tree_error(TREE_ERR_MISSING_CHILD);
            break;
        }
    }

    if (check_ctx.err != TREE_OK) {
        PRINTF("Node tree check failed with code %d\n", check_ctx.err);
    }

    // Every node has arrived and the verdict is known, so the tracking array is no longer needed.
    // Free it here to give the heap back before metadata parsing starts.
    if (check_ctx.nodes != NULL) {
        app_mem_free(check_ctx.nodes);
        check_ctx.nodes = NULL;
    }
    check_ctx.nodes_count = 0;

    return check_ctx.err;
}

MUST_CHECK bool tree_can_clear_sign(void) {
    return check_ctx.err == TREE_OK;
}

MUST_CHECK bool tree_error_is_impossible(int err) {
    switch (err) {
        case TREE_ERR_NODE_ID_OUT_OF_RANGE:
        case TREE_ERR_DUPLICATE_NODE:
        case TREE_ERR_CHILD_OUT_OF_RANGE:
        case TREE_ERR_SELF_CHILD:
        case TREE_ERR_ROOT_CLAIMED:
        case TREE_ERR_DUPLICATE_CLAIM:
            return true;
        default:
            return false;
    }
}
