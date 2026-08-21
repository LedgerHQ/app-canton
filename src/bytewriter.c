#include "bytewriter.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "constants.h"
#include "mem.h"
#include "ledger_assert.h"

void bw_init(ByteWriter *bw, void *buf, size_t cap) {
    LEDGER_ASSERT(bw != NULL, "NULL ByteWriter pointer passed to bw_init");
    LEDGER_ASSERT(buf != NULL, "NULL buffer pointer passed to bw_init");
    LEDGER_ASSERT(cap > 0, "Zero capacity passed to bw_init");

    bw->base = bw->ptr = (uint8_t *) buf;
    bw->end = bw->base + cap;
    bw->overflow = false;
}

MUST_CHECK size_t bw_size(const ByteWriter *bw) {
    LEDGER_ASSERT(bw != NULL, "NULL ByteWriter pointer passed to bw_size");

    return (size_t) (bw->ptr - bw->base);
}

void bw_put(ByteWriter *bw, const void *p, size_t n) {
    LEDGER_ASSERT(bw != NULL, "NULL ByteWriter pointer passed to bw_put");
    LEDGER_ASSERT(p != NULL, "NULL pointer passed to bw_put");

    if (bw->overflow || bw->ptr + n > bw->end) {
        bw->overflow = true;
        return;
    }
    memcpy(bw->ptr, p, n);
    bw->ptr += n;
}

void bw_put_byte(ByteWriter *bw, uint8_t b) {
    bw_put(bw, &b, 1);
}

// Big‑endian helpers
void bw_put_u32_be(ByteWriter *bw, uint32_t v) {
    uint8_t t[UINT32_T_LEN] = {(uint8_t) (v >> 24),
                               (uint8_t) (v >> 16),
                               (uint8_t) (v >> 8),
                               (uint8_t) v};
    bw_put(bw, t, UINT32_T_LEN);
}
void bw_put_u64_be(ByteWriter *bw, uint64_t v) {
    uint8_t t[UINT64_T_LEN] = {(uint8_t) (v >> 56),
                               (uint8_t) (v >> 48),
                               (uint8_t) (v >> 40),
                               (uint8_t) (v >> 32),
                               (uint8_t) (v >> 24),
                               (uint8_t) (v >> 16),
                               (uint8_t) (v >> 8),
                               (uint8_t) v};
    bw_put(bw, t, UINT64_T_LEN);
}
