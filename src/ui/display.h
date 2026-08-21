#pragma once

#include <stdbool.h>  // bool
#include "utils.h"
#include "glyphs.h"

#if defined(TARGET_NANOX) || defined(TARGET_NANOS2)
#define ICON_APP_CANTON  C_app_canton_14px
#define ICON_APP_HOME    C_home_canton_14px
#define ICON_APP_WARNING C_icon_warning
#elif defined(TARGET_STAX) || defined(TARGET_FLEX)
#define ICON_APP_CANTON  C_app_canton_64px
#define ICON_APP_HOME    ICON_APP_CANTON
#define ICON_APP_WARNING LARGE_WARNING_ICON
#elif defined(TARGET_APEX_P)
#define ICON_APP_CANTON  C_app_canton_48px
#define ICON_APP_HOME    ICON_APP_CANTON
#define ICON_APP_WARNING LARGE_WARNING_ICON
#endif
/**
 * Display party id on the device and ask confirmation to export.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
MUST_CHECK int ui_display_party_id(void);

/**
 * Display transaction information on the device and ask confirmation to sign.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
MUST_CHECK int ui_display_transaction(void);

/**
 * Display blind-sign transaction information on the device and ask confirmation to sign.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
MUST_CHECK int ui_display_blind_signed_transaction(void);

/**
 * Display an error message indicating that blind signing is not enabled.
 *
 * @return Status word indicating incorrect data.
 */
MUST_CHECK int ui_error_blind_signing(void);
