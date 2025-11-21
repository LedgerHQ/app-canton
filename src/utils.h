#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include "constants.h"

// Mark functions whose return values must be checked.
// So it should be used on all functions that return something other
// than void.
#define MUST_CHECK __attribute__((warn_unused_result))

/**
 * Compute SHA-256 fingerprint with a purpose byte as domain separator.
 *
 * @param[in]  purpose   A single-byte value indicating the hash purpose
 * @param[in]  data      Pointer to input buffer
 * @param[in]  data_len  Length of input buffer
 * @param[out] out       32-byte output buffer for the digest
 */
void canton_fingerprint(uint8_t purpose,
                        const uint8_t *data,
                        size_t data_len,
                        uint8_t out[SHA256_HASH_LEN]);

/**
 * @brief Compute a fingerprint and add a 2-byte prefix to indicate SHA-256 with length.
 *
 * @param[in]  purpose   A single-byte value indicating the hash purpose
 * @param[in]  data      Pointer to input buffer
 * @param[in]  data_len  Length of input buffer
 * @param[out] out       34-byte output buffer for the prefixed digest
 */
void canton_hash(uint8_t purpose,
                 const uint8_t *data,
                 size_t data_len,
                 uint8_t out[CANTON_HASH_LEN]);

/**
 * @brief Safe snprintf macro that disables -Wformat for the snprintf call.
 * We have custom formats like %H that would trigger warnings).
 */
#define SNPRINTF(str, size, format, ...)                                              \
    do {                                                                              \
        _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wformat\"") \
            snprintf(str, size, format, __VA_ARGS__);                                 \
        _Pragma("GCC diagnostic pop")                                                 \
    } while (0)

/**
 * @brief Check if a character is a digit.
 *
 * @param c Character to check.
 * @return true if the character is a digit, false otherwise.
 */
MUST_CHECK bool is_digit(char c);

/**
 * @brief Convert a string of digits to an integer.
 *        Returns 0 if the string contains non-digit characters.
 *
 * @param str Null-terminated string to convert.
 * @return The integer value, or 0 on error.
 */
MUST_CHECK int atoint(const char *str);

/**
 * @brief Convert a string of digits to an unsigned long long integer.
 *        Returns 0 if the string contains non-digit characters.
 *
 * @param str Null-terminated string to convert.
 * @return The unsigned long long integer value, or 0 on error.
 */
MUST_CHECK uint64_t atoull(const char *str);
