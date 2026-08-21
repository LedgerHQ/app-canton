#pragma once

#include "utils.h"
#include "types.h"

MUST_CHECK bool init_transaction_pairs(transaction_ctx_t *tx_info, size_t count);
MUST_CHECK int parse_node_for_display(buffer_t *buf);
MUST_CHECK int parse_input_contract_for_display(buffer_t *buf);
void cleanup_display_items(void);
void reset_display_parser_state(void);
