#include "os.h"
#include "io.h"
#include "display.h"
#include <stdbool.h>
#include "menu.h"
#include "nbgl_use_case.h"
#include "status_words.h"
#include "utils.h"

#ifdef SCREEN_SIZE_WALLET
static void ui_error_blind_signing_choice(bool confirm) {
    if (confirm) {
        ui_menu_settings();
    } else {
        ui_menu_main();
    }
}
#endif

MUST_CHECK int ui_error_blind_signing(void) {
#ifdef SCREEN_SIZE_WALLET
    nbgl_useCaseChoice(&ICON_APP_WARNING,
                       "This transaction cannot be clear-signed",
                       "Enable blind signing in the settings to sign this transaction.",
                       "Go to settings",
                       "Reject transaction",
                       ui_error_blind_signing_choice);
#else
    nbgl_useCaseAction(&C_Alert_circle_14px,
                       "Blind signing must\nbe enabled in\nsettings",
                       NULL,
                       ui_menu_main);
#endif
    return io_send_sw(SWO_INCORRECT_DATA);
}
