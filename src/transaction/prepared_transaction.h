#pragma once

#include <stdint.h>  // uint*_t
#include "utils.h"

void process_prepared_tx_init();
MUST_CHECK int process_prepared_tx_part(buffer_t *buf);
