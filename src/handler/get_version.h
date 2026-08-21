#pragma once

#include "utils.h"

/**
 * Handler gor GET_VERSION command. Send APDU response with version
 * of the application.
 *
 * @see MAJOR_VERSION, MINOR_VERSION and PATCH_VERSION in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
MUST_CHECK int handler_get_version(void);
