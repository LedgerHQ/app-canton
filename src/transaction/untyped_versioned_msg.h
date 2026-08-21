#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include <string.h>

#include "os.h"
#include "cx.h"
#include "ledger_assert.h"

#include "party_id.h"
#include "utils.h"

#include "tx_types.h"

MUST_CHECK bool process_untyped_versioned_msg_tx_init(buffer_t *cdata);
MUST_CHECK int process_untyped_versioned_msg_tx(buffer_t *buf);
