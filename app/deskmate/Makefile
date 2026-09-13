############################################################################
# vendor/allwinnertech/apps/deskmate/Makefile
############################################################################

include $(APPDIR)/Make.defs
include $(APPDIR)/vendor/allwinnertech/Common.mk

MAINSRC = deskmate_main.c
CSRCS   = dm_state.c dm_face.c dm_roll.c dm_ui_focus.c dm_ui_shells.c \
          dm_ui_health.c

PROGNAME  = $(CONFIG_DESKMATE_APP_PROGNAME)
PRIORITY  = $(CONFIG_DESKMATE_APP_PRIORITY)
STACKSIZE = $(CONFIG_DESKMATE_APP_STACKSIZE)
MODULE    = $(CONFIG_DESKMATE_APP)

include $(APPDIR)/Application.mk
