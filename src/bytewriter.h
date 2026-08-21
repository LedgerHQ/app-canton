#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "utils.h"

typedef struct {
    uint8_t *base, *ptr, *end;
    bool overflow;
} ByteWriter;

void bw_init(ByteWriter *bw, void *buf, size_t cap);
MUST_CHECK size_t bw_size(const ByteWriter *bw);
void bw_put(ByteWriter *bw, const void *p, size_t n);
void bw_put_byte(ByteWriter *bw, uint8_t b);
void bw_put_u32_be(ByteWriter *bw, uint32_t v);
void bw_put_u64_be(ByteWriter *bw, uint64_t v);
