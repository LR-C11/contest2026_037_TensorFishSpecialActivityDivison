# vendor/allwinnertech/apps/deskmate/CMakeLists.txt

if(CONFIG_DESKMATE_APP)
  nuttx_add_application(
    NAME deskmate
    PRIORITY ${CONFIG_DESKMATE_APP_PRIORITY}
    STACKSIZE ${CONFIG_DESKMATE_APP_STACKSIZE}
    SRCS
      deskmate_main.c
      dm_state.c
      dm_face.c
      dm_roll.c
      dm_ui_focus.c
      dm_ui_shells.c
      dm_ui_health.c
      dm_ui_wifi.c
      dm_wifi.c
      dm_ui_word.c
      dm_word_bank.c
    INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})
endif()
