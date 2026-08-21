#pragma once

#include "utils.h"
#include "app_mem_utils.h"

#define app_mem_alloc(size)   APP_MEM_ALLOC(size)
#define pb_realloc(ptr, size) APP_MEM_REALLOC(ptr, size)
#define pb_free(ptr)          app_mem_free(ptr)

MUST_CHECK bool app_mem_init(void);
void app_mem_free(void *ptr);
