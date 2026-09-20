############################################################################
# vendor/allwinnertech/apps/deskmate/Makefile
############################################################################

include $(APPDIR)/Make.defs
include $(APPDIR)/vendor/allwinnertech/Common.mk

MAINSRC = deskmate_main.c
CSRCS   = dm_state.c dm_face.c dm_roll.c dm_ui_focus.c dm_ui_shells.c \
          dm_ui_health.c dm_ui_wifi.c dm_wifi.c dm_ui_word.c dm_word_bank.c \
          dm_ui_tools.c dm_ui_life.c dm_ui_fun.c dm_ui_med.c \
          dm_ui_note.c dm_pinyin.c dm_ui_2048.c dm_ui_chat.c dm_chat_ai.c \
          dm_ui_mbti.c

PROGNAME  = $(CONFIG_DESKMATE_APP_PROGNAME)
PRIORITY  = $(CONFIG_DESKMATE_APP_PRIORITY)
STACKSIZE = $(CONFIG_DESKMATE_APP_STACKSIZE)
MODULE    = $(CONFIG_DESKMATE_APP)

include $(APPDIR)/Application.mk
