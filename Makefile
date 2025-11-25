# ****************************************************************************
#    Ledger App Boilerplate
#    (c) 2023 Ledger SAS.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# ****************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif

include $(BOLOS_SDK)/Makefile.target

########################################
#        Mandatory configuration       #
########################################
# Application name
APPNAME = "Canton"

# Application version
APPVERSION_M = 3
APPVERSION_N = 1
APPVERSION_P = 0
APPVERSION = "$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)"

# Application source files
APP_SOURCE_PATH += src

# Application icons following guidelines:
# https://developers.ledger.com/docs/embedded-app/design-requirements/#device-icon
ICON_NANOX = icons/app_canton_14px.gif
ICON_NANOSP = icons/app_canton_14px.gif
ICON_STAX = icons/app_canton_32px.gif
ICON_FLEX = icons/app_canton_40px.gif
ICON_APEX_P = icons/app_canton_32px_apex.png

# With the Nano NBGL Design, the Home Screen icon is the reverse of the App icon:
# It should be on white background, with rounded corners.
# This definition allows SDK Makefiles to automatically generate it based on the App icon.
# Please note that the icon is dynamically generated, and declared in the .gitignore to avoid storing it.
ICON_HOME_NANO = glyphs/home_canton_14px.gif

# Attestations for challenge signature during party onboarding.
PROD_CANTON_PRIVATE_KEY?=0
ifneq ($(PROD_CANTON_PRIVATE_KEY),0)
    DEFINES += PROD_PRIVATE_KEY=${PROD_CANTON_PRIVATE_KEY}
endif

# Application allowed derivation curves.
CURVE_APP_LOAD_PARAMS = ed25519

# Application allowed derivation paths.
PATH_APP_LOAD_PARAMS = "44'/6767'"

# Variants list
VARIANT_PARAM = COIN
VARIANT_VALUES = CC

# Enabling DEBUG flag will enable PRINTF and disable optimizations
#DEBUG = 1

########################################
#     Application custom permissions   #
########################################
# See SDK `include/appflags.h` for the purpose of each permission
#HAVE_APPLICATION_FLAG_DERIVE_MASTER = 1
#HAVE_APPLICATION_FLAG_GLOBAL_PIN = 1
#HAVE_APPLICATION_FLAG_BOLOS_SETTINGS = 1
#HAVE_APPLICATION_FLAG_LIBRARY = 1

########################################
# Application communication interfaces #
########################################
ENABLE_BLUETOOTH = 1
#ENABLE_NFC = 1
ENABLE_NBGL_FOR_NANO_DEVICES = 1

########################################
#         NBGL custom features         #
########################################
ENABLE_NBGL_QRCODE = 1
#ENABLE_NBGL_KEYBOARD = 1
#ENABLE_NBGL_KEYPAD = 1

########################################
#          Features disablers          #
########################################
# These advanced settings allow to disable some feature that are by
# default enabled in the SDK `Makefile.standard_app`.
#DISABLE_STANDARD_APP_FILES = 1
#DISABLE_DEFAULT_IO_SEPROXY_BUFFER_SIZE = 1 # To allow custom size declaration
#DISABLE_STANDARD_APP_DEFINES = 1 # Will set all the following disablers
#DISABLE_STANDARD_SNPRINTF = 1
#DISABLE_STANDARD_USB = 1
#DISABLE_STANDARD_WEBUSB = 1
#DISABLE_DEBUG_LEDGER_ASSERT = 1
#DISABLE_DEBUG_THROW = 1

ENABLE_DYNAMIC_ALLOC = 1
ifneq ($(DEBUG), 0)
    MEMORY_PROFILING ?= 0
    ifneq ($(MEMORY_PROFILING),0)
        DEFINES += HAVE_MEMORY_PROFILING
    endif
endif

DEFINES += PB_ENABLE_MALLOC=1

include vendor/nanopb/extra/nanopb.mk

INCLUDES_PATH += $(NANOPB_DIR) . proto

# DEFINES   += PB_NO_ERRMSG=1
DEFINES   += PB_ENABLE_ERRORS=1
SOURCE_FILES += $(NANOPB_CORE)
APP_SOURCE_PATH += proto

include $(BOLOS_SDK)/Makefile.standard_app
