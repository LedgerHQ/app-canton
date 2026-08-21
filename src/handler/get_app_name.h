#pragma once

#include "os.h"
#include "utils.h"

/**
 * Handler for GET_APP_NAME command. Send APDU response with ASCII
 * encoded name of the application.
 *
 * @see variable APPNAME in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
MUST_CHECK int handler_get_app_name(void);
