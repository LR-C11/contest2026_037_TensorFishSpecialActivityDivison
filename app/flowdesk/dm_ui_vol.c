/*
 * dm_ui_vol.c — Volume settings (large touch targets, no overlap)
 * 320x240 screen, dock hidden
 */

#include "deskmate.h"
#include "dm_audio_vol.h"
#include "dm_audio_route.h"
#include "dm_voice.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

static lv_obj_t *s_pct;
static lv_obj_t *s_hint;
static lv_obj_t *s_slider;
static lv_obj_t *s_mute_btn;
static lv_obj_t *s_out_name;
static bool s_syncing;
static volatile int s_test_busy;
static volatile int s_test_done;
static volatile int s_test_ok;

static void *test_beep_worker(void *arg)
{
  (void)arg;
  s_test_ok = dm_vol_test_sample();
  s_test_done = 1;
  return NULL;
}

static void refresh(void)
{
  char buf[32];
  int v = dm_vol_get();

  snprintf(buf, sizeof(buf), "%d%%", v);
  if (s_pct)
    {
      lv_label_set_text(s_pct, buf);
    }
  if (s_hint)
    {
      if (dm_vol_is_mute())
        {
          lv_label_set_text(s_hint, dm_t("已静音", "Muted"));
        }
      else
        {
          lv_label_set_text(s_hint, dm_t("拖动滑杆调音量", "Drag slider"));
        }
    }
  if (s_out_name)
    {
      lv_label_set_text(s_out_name, dm_out_name(dm_out_get()));
    }
  if (s_slider)
    {
      s_syncing = true;
      lv_slider_set_value(s_slider, v, LV_ANIM_OFF);
      s_syncing = false;
    }
  if (s_mute_btn)
    {
      lv_obj_t *lab = lv_obj_get_child(s_mute_btn, 0);
      if (lab)
        {
          lv_label_set_text(lab, dm_vol_is_mute() ? dm_t("取消静音", "Unmute")
                                                  : dm_t("静音", "Mute"));
        }
    }
}

static void slider_cb(lv_event_t *e)
{
  (void)e;
  if (s_syncing || !s_slider)
    {
      return;
    }
  (void)dm_vol_set((int)lv_slider_get_value(s_slider));
  refresh();
}

static void btn_minus(lv_event_t *e)
{
  (void)e;
  (void)dm_vol_set(dm_vol_get() - 10);
  refresh();
}

static void btn_plus(lv_event_t *e)
{
  (void)e;
  (void)dm_vol_set(dm_vol_get() + 10);
  refresh();
}

static void btn_mute(lv_event_t *e)
{
  (void)e;
  (void)dm_vol_set_mute(!dm_vol_is_mute());
  refresh();
}

static void btn_test(lv_event_t *e)
{
  pthread_t th;
  pthread_attr_t attr;
  int ret;

  (void)e;
  if (s_test_busy)
    {
      return;
    }

  if (dm_vol_is_mute())
    {
      (void)dm_vol_set_mute(0);
    }

  s_test_busy = 1;
  s_test_done = 0;
  if (s_hint)
    {
      lv_label_set_text(s_hint, dm_t("语音试听中…", "Playing sample audio…"));
    }

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, DM_VOICE_NET_WORKER_STACK);
  ret = pthread_create(&th, &attr, test_beep_worker, NULL);
  pthread_attr_destroy(&attr);
  if (ret == 0)
    {
      pthread_detach(th);
    }
  else
    {
      s_test_busy = 0;
      if (s_hint)
        {
          lv_label_set_text(s_hint,
                            dm_t("试听启动失败", "Could not start test tone"));
        }
    }
}

void dm_volume_tick(void)
{
  if (!s_test_done)
    {
      return;
    }

  s_test_done = 0;
  s_test_busy = 0;
  if (s_hint)
    {
      if (s_test_ok == 0)
        {
          lv_label_set_text(s_hint,
                            dm_t("试听完成（预置音色或缓存）",
                                 "Preview finished (bundled or cached)"));
        }
      else if (s_test_ok > 0)
        {
          lv_label_set_text(s_hint,
                            dm_t("云端语音不可用，已回退蜂鸣音",
                                 "Cloud sample unavailable; played fallback tone"));
        }
      else
        {
          lv_label_set_text(s_hint,
                            dm_t("音频忙或播放失败",
                                 "Audio busy or playback failed"));
        }
    }
}

static void btn_out(lv_event_t *e)
{
  int ch = (int)(intptr_t)lv_event_get_user_data(e);
  (void)e;
  dm_out_set(ch);
  refresh();
  if (s_hint)
    {
      lv_label_set_text(s_hint, dm_out_name(dm_out_get()));
    }
}

static void btn_back(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_SETTINGS);
}

void dm_create_volume(void)
{
  lv_obj_t *page, *b;

  page = lv_obj_create(g_dm_root);
  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(MD_SURFACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_VOLUME] = page;

  /* back */
  b = dm_btn(page, "←", "<", 40, 28, MD_SURFACE_HIGH, MD_ON_SURFACE,
             btn_back, NULL);
  lv_obj_set_pos(b, 10, 8);

  {
    lv_obj_t *t = dm_lbl(page, "音量", "Volume", g_dm_font_m, MD_ON_SURFACE);
    lv_obj_set_pos(t, 58, 10);
  }

  /* big percent */
  s_pct = dm_lbl(page, "80%", "80%", g_dm_font_xl, MD_PRIMARY);
  lv_obj_set_style_text_align(s_pct, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(s_pct, 200);
  lv_obj_set_pos(s_pct, 60, 40);

  s_hint = dm_lbl(page, "拖动滑杆调音量", "Drag slider", g_dm_font_s,
                  MD_ON_SURFACE_VARIANT);
  lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(s_hint, 280);
  lv_obj_set_pos(s_hint, 20, 86);

  /* slider full width */
  s_slider = lv_slider_create(page);
  lv_obj_set_size(s_slider, 280, 16);
  lv_obj_set_pos(s_slider, 20, 110);
  lv_slider_set_range(s_slider, 0, 100);
  lv_obj_set_style_bg_color(s_slider, lv_color_hex(MD_SURFACE_HIGHEST),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_slider, lv_color_hex(MD_PRIMARY),
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s_slider, lv_color_hex(MD_ON_PRIMARY),
                            LV_PART_KNOB);
  lv_obj_set_style_pad_all(s_slider, 8, LV_PART_KNOB);
  lv_obj_add_event_cb(s_slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

  /* -10 / +10 */
  b = dm_btn(page, "−10", "-10", 80, 32, MD_SURFACE_HIGH, MD_ON_SURFACE,
             btn_minus, NULL);
  lv_obj_set_pos(b, 20, 138);
  b = dm_btn(page, "＋10", "+10", 80, 32, MD_SURFACE_HIGH, MD_ON_SURFACE,
             btn_plus, NULL);
  lv_obj_set_pos(b, 220, 138);

  {
    lv_obj_t *ol = dm_lbl(page, "输出", "Output", g_dm_font_s,
                          MD_ON_SURFACE_VARIANT);
    lv_obj_set_pos(ol, 110, 142);
    s_out_name = dm_lbl(page, "自动", "Auto", g_dm_font_s, MD_PRIMARY);
    lv_obj_set_pos(s_out_name, 160, 142);
  }

  /* output: 2 big buttons */
  b = dm_btn(page, "自动", "Auto", 130, 34, MD_SECONDARY_CONTAINER,
             MD_ON_SECONDARY_CONT, btn_out, (void *)(intptr_t)DM_OUT_AUTO);
  lv_obj_set_pos(b, 30, 172);
  b = dm_btn(page, "喇叭", "Speaker", 130, 34, MD_SECONDARY_CONTAINER,
             MD_ON_SECONDARY_CONT, btn_out, (void *)(intptr_t)DM_OUT_SPEAKER);
  lv_obj_set_pos(b, 160, 172);

  /* mute / test */
  s_mute_btn = dm_btn(page, "静音", "Mute", 140, 36, MD_SURFACE_HIGH,
                      MD_ON_SURFACE, btn_mute, NULL);
  lv_obj_set_pos(s_mute_btn, 20, 216);
  b = dm_btn(page, "试听", "Test", 140, 36, MD_PRIMARY, MD_ON_PRIMARY,
             btn_test, NULL);
  lv_obj_set_pos(b, 160, 216);

  dm_vol_init();
  dm_out_init();
  refresh();
}

#endif /* CONFIG_DESKMATE_APP */
