/****************************************************************************
 * deskmate_main.c — entry & LVGL bring-up
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/boardctl.h>

#ifdef CONFIG_DESKMATE_APP

#ifndef NEED_BOARDINIT
#  if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
#    define NEED_BOARDINIT 1
#  endif
#endif

static void tick_cb(lv_timer_t *t)
{
  (void)t;
  dm_tick();
}

int deskmate_main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

  (void)argc;
  (void)argv;

  if (lv_is_initialized())
    {
      return -1;
    }

  /* LCD is owned by deskmate; luncher_mini is not started from rcS
   * when CONFIG_DESKMATE_APP is enabled. killall is unavailable on
   * this NuttX image, so do not rely on it. */

#ifdef NEED_BOARDINIT
  boardctl(BOARDIOC_INIT, 0);
#endif

  lv_init();
  lv_nuttx_dsc_init(&info);
#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif
#ifdef CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH
  info.input_path = CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH;
#endif
  lv_nuttx_init(&info, &result);
  usleep(100000);
  if (!result.disp)
    {
      return 1;
    }

  dm_init_fonts();

  g_dm_root = lv_obj_create(lv_screen_active());
  lv_obj_set_size(g_dm_root, DM_SCR_W, DM_SCR_H);
  lv_obj_center(g_dm_root);
  lv_obj_set_style_bg_color(g_dm_root, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_dm_root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_dm_root, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_dm_root, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_dm_root, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_dm_root, LV_OBJ_FLAG_SCROLLABLE);

  dm_create_focus_home();
  dm_create_focus_run();
  dm_create_chat();
  dm_create_health();
  dm_create_supervise();
  dm_create_note();
  dm_create_features();
  dm_create_word();
  dm_create_settings();
  dm_create_wifi();
  dm_create_dock();
  dm_show(PAGE_FOCUS_HOME);

  lv_timer_create(tick_cb, DM_TICK_MS, NULL);

  LV_LOG_USER("Deskmate " DM_VER " focus-home zh=%d", (int)g_dm.zh);

  while (true)
    {
      uint32_t idle = lv_timer_handler();
      usleep(idle ? idle * 1000 : 5000);
    }

  return 0;
}

#endif /* CONFIG_DESKMATE_APP */
