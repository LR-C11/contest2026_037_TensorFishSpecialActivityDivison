/****************************************************************************
 * desk_companion_main.c — entry (v0.6 multi-page)
 ****************************************************************************/

#include "desk_companion.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/boardctl.h>
#include <sensor/humi.h>
#include <sensor/temp.h>
#include <uORB/uORB.h>

#ifdef CONFIG_DESK_COMPANION_APP

void desk_create_home(void);
void desk_create_wifi(void);
void desk_create_bt(void);
void desk_create_note(void);
void desk_create_about(void);
void desk_create_focus(void);
void desk_create_kb(void);
void desk_tick(void);
void desk_init_fonts(void);
void desk_show(desk_page_t p);

static void tick_cb(lv_timer_t *t)
{
  (void)t;
  desk_tick();
}

int desk_companion_main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  (void)argc;
  (void)argv;

  if (lv_is_initialized())
    return -1;

  /* Stop official launcher so it does not fight us for the LCD */
#ifdef CONFIG_SYSTEM_SYSTEM
  (void)system("killall luncher_mini > /dev/null 2>&1");
#endif

#ifdef CONFIG_BOARDCTL
#  ifndef CONFIG_NSH_ARCHINIT
  boardctl(BOARDIOC_INIT, 0);
#  endif
#endif

  lv_init();
  lv_nuttx_dsc_init(&info);
#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif
#ifdef CONFIG_INPUT_TOUCHSCREEN
  info.input_path = CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH;
#endif
  lv_nuttx_init(&info, &result);
  usleep(100000);
  if (!result.disp)
    {
      LV_LOG_ERROR("display init failed");
      return 1;
    }

  desk_init_fonts();

  g_root = lv_obj_create(lv_screen_active());
  lv_obj_set_size(g_root, 320, 240);
  lv_obj_center(g_root);
  lv_obj_set_style_bg_color(g_root, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_root, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_root, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_root, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);

  desk_create_home();
  desk_create_wifi();
  desk_create_bt();
  desk_create_note();
  desk_create_about();
  desk_create_focus();
  desk_create_kb();
  desk_show(PAGE_HOME);

  g_temp_sub = orb_subscribe_multi(ORB_ID(sensor_temp), 0);
  g_humi_sub = orb_subscribe_multi(ORB_ID(sensor_humi), 0);
  lv_timer_create(tick_cb, 250, NULL);

  LV_LOG_USER("DeskMate %s pages=home/focus/wifi/bt/note/about zh=%d",
              DESK_VER, (int)g_zh);

  while (true)
    {
      uint32_t idle = lv_timer_handler();
      usleep(idle ? idle * 1000 : 5000);
    }

  if (g_temp_sub >= 0)
    orb_unsubscribe(g_temp_sub);
  if (g_humi_sub >= 0)
    orb_unsubscribe(g_humi_sub);
  return 0;
}

#endif
