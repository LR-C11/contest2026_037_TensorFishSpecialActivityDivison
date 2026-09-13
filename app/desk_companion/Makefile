############################################################################
# vendor/allwinnertech/apps/desk_companion/Makefile
############################################################################

include $(APPDIR)/Make.defs
include $(APPDIR)/vendor/allwinnertech/Common.mk

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/frameworks/connectivity/bluetooth/framework/include
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/system/uorb

MAINSRC = desk_companion_main.c
CSRCS   = desk_state.c desk_face.c desk_ui_home.c desk_ui_wifi.c \
          desk_ui_bt.c desk_ui_note_about.c desk_ui_focus.c

PROGNAME  = $(CONFIG_DESK_COMPANION_APP_PROGNAME)
PRIORITY  = $(CONFIG_DESK_COMPANION_APP_PRIORITY)
STACKSIZE = $(CONFIG_DESK_COMPANION_APP_STACKSIZE)
MODULE    = $(CONFIG_DESK_COMPANION_APP)

include $(APPDIR)/Application.mk
