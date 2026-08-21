/**
 * Dynamic allocator that uses a fixed-length buffer that is hopefully big enough
 *
 * The two functions alloc & dealloc use the buffer as a simple stack.
 * Especially useful when an unpredictable amount of data will be received and have to be stored
 * during the transaction but discarded right after.
 */

#include <stdint.h>
#include "mem.h"
#include "app_mem_utils.h"

#define SIZE_MEM_BUFFER (1024 * 3)

static uint8_t mem_buffer[SIZE_MEM_BUFFER] __attribute__((aligned(sizeof(intmax_t))));

MUST_CHECK bool app_mem_init(void) {
    void *buf = mem_buffer;
    size_t buf_size = sizeof(mem_buffer);
    return mem_utils_init(buf, buf_size);
}

void app_mem_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    APP_MEM_FREE(ptr);
}
