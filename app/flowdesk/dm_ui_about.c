/*
 * dm_ui_about.c — About page (BandLingo-style dark cards)
 * PAGE_ABOUT
 */

#include "deskmate.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>

static lv_obj_t *s_about;

static void about_back(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_SETTINGS);
}

void dm_create_about(void)
{
  lv_obj_t *title, *back, *card1, *card2, *lab, *icon, *dev;
  lv_img_dsc_t *img = NULL;

  s_about = lv_obj_create(g_dm_root);
  lv_obj_set_size(s_about, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(s_about, 0, 0);
  lv_obj_set_style_bg_color(s_about, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_about, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_about, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_about, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_about, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_ABOUT] = s_about;

  back = dm_btn(s_about, "←", "<", 34, 24, 0x1A1A1A, 0xE6E0E9, about_back, NULL);
  lv_obj_set_pos(back, 8, 8);

  title = dm_lbl(s_about, "关于 FlowDesk", "About FlowDesk", g_dm_font_m,
                 0x7DD3FC);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(title, 220);
  lv_obj_set_pos(title, 50, 10);

  /* app card */
  card1 = lv_obj_create(s_about);
  lv_obj_set_size(card1, 296, 72);
  lv_obj_set_pos(card1, 12, 48);
  lv_obj_set_style_bg_color(card1, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card1, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(card1, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(card1, 14, LV_PART_MAIN);
  lv_obj_set_style_pad_all(card1, 10, LV_PART_MAIN);
  lv_obj_clear_flag(card1, LV_OBJ_FLAG_SCROLLABLE);

  icon = lv_obj_create(card1);
  lv_obj_set_size(icon, 48, 48);
  lv_obj_set_pos(icon, 4, 12);
  lv_obj_set_style_bg_color(icon, lv_color_hex(0x2C2C2E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(icon, 24, LV_PART_MAIN);
  lv_obj_set_style_border_width(icon, 0, LV_PART_MAIN);
  {
    lv_obj_t *t = dm_lbl(icon, "F", "F", g_dm_font_l, 0x7DD3FC);
    lv_obj_center(t);
  }

  lab = dm_lbl(card1, "FlowDesk", "FlowDesk", g_dm_font_m, 0xFFFFFF);
  lv_obj_set_pos(lab, 64, 12);
  {
    lv_obj_t *v = dm_lbl(card1, "版本 " DM_VER, "Version " DM_VER,
                         g_dm_font_s, 0x8E8E93);
    lv_obj_set_pos(v, 64, 36);
  }

  /* developer card */
  card2 = lv_obj_create(s_about);
  lv_obj_set_size(card2, 296, 110);
  lv_obj_set_pos(card2, 12, 132);
  lv_obj_set_style_bg_color(card2, lv_color_hex(0x1C1C1E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card2, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(card2, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(card2, 14, LV_PART_MAIN);
  lv_obj_set_style_pad_all(card2, 10, LV_PART_MAIN);
  lv_obj_clear_flag(card2, LV_OBJ_FLAG_SCROLLABLE);

  lab = dm_lbl(card2, "开发者信息  Developers", "Developers", g_dm_font_s,
               0xE6E0E9);
  lv_obj_set_pos(lab, 4, 2);

  /* author avatar from resource */
  {
    static lv_img_dsc_t s_author_img;
    FILE *f = fopen("/resource/imgs/author.png", "rb");
    if (!f)
      {
        f = fopen("/data/imgs/author.png", "rb");
      }
    if (f)
      {
        fclose(f);
        /* LVGL can load PNG via image decoder if enabled; use img obj with src path */
        icon = lv_img_create(card2);
        lv_img_set_src(icon, "A:/resource/imgs/author.png");
        lv_obj_set_size(icon, 40, 40);
        lv_obj_set_pos(icon, 8, 36);
        (void)s_author_img;
        (void)img;
      }
    else
      {
        icon = lv_obj_create(card2);
        lv_obj_set_size(icon, 40, 40);
        lv_obj_set_pos(icon, 8, 36);
        lv_obj_set_style_bg_color(icon, lv_color_hex(0x3A5A8C), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(icon, 20, LV_PART_MAIN);
        lv_obj_set_style_border_width(icon, 0, LV_PART_MAIN);
      }
  }

  dev = dm_lbl(card2, "@LR.C", "@LR.C", g_dm_font_m, 0xFFFFFF);
  lv_obj_set_pos(dev, 58, 36);
  {
    lv_obj_t *role = dm_lbl(card2, "主开发者", "Lead developer", g_dm_font_s,
                            0x8E8E93);
    lv_obj_set_pos(role, 58, 60);
  }

  {
    lv_obj_t *foot = dm_lbl(s_about, "基于 openvela · Gemini-S1",
                            "openvela · Gemini-S1", g_dm_font_s, 0x636366);
    lv_obj_set_style_text_align(foot, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(foot, 280);
    lv_obj_set_pos(foot, 20, 218);
  }
}

#endif /* CONFIG_DESKMATE_APP */
