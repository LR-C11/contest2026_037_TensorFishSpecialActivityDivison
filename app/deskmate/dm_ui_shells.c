/****************************************************************************
 * dm_ui_shells.c — skeleton pages + bottom dock
 ****************************************************************************/

#include "deskmate.h"

#ifdef CONFIG_DESKMATE_APP

static lv_obj_t *s_dock_btns[5];
static const dm_page_t s_dock_target[5] = {
  PAGE_FOCUS_HOME, PAGE_CHAT, PAGE_HEALTH, PAGE_FEATURES, PAGE_SETTINGS
};

static lv_obj_t *mk_shell(dm_page_t id, const char *zh_title,
                          const char *en_title, const char *zh_hint,
                          const char *en_hint)
{
  lv_obj_t *page = lv_obj_create(g_dm_root);
  lv_obj_t *title;
  lv_obj_t *card;
  lv_obj_t *hint;
  lv_obj_t *soon;

  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

  title = dm_lbl(page, zh_title, en_title, g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 12, 10);

  card = lv_obj_create(page);
  lv_obj_set_size(card, 296, 110);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_set_style_bg_color(card, lv_color_hex(C_BTN), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 16, LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  hint = dm_lbl(card, zh_hint, en_hint, g_dm_font_s, C_DIM);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(hint, 260);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_center(hint);

  soon = dm_lbl(page, "骨架已就绪，能力后续填充",
                "Skeleton ready — features later", g_dm_font_s, C_MUTED);
  lv_obj_set_style_text_align(soon, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(soon, LV_ALIGN_TOP_MID, 0, 172);

  g_dm_pages[id] = page;
  return page;
}

void dm_create_chat(void)
{
  mk_shell(PAGE_CHAT, "聊天", "Chat",
           "无聊时我会陪你聊。\n完整对话能力稍后接入。",
           "I'll keep you company.\nFull chat comes later.");
}

static void back_features(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_FEATURES);
}

void dm_create_supervise(void)
{
  mk_shell(PAGE_SUPERVISE, "监督", "Supervise",
           "叫我盯着你完成一件事，\n尽量不被别的打扰。",
           "Ask me to watch one task with you.");
  if (g_dm_pages[PAGE_SUPERVISE])
    {
      lv_obj_t *b = dm_btn(g_dm_pages[PAGE_SUPERVISE], "←", "<", 36, 24,
                           C_BTN, C_MUTED, back_features, NULL);
      lv_obj_set_pos(b, 8, 8);
    }
}

void dm_create_note(void)
{
  mk_shell(PAGE_NOTE, "备忘", "Notes",
           "帮你记一下东西。\n列表与持久化骨架已就绪。",
           "I'll remember things for you.");
  if (g_dm_pages[PAGE_NOTE])
    {
      lv_obj_t *b = dm_btn(g_dm_pages[PAGE_NOTE], "←", "<", 36, 24, C_BTN,
                           C_MUTED, back_features, NULL);
      lv_obj_set_pos(b, 8, 8);
    }
}

static void fn_row_cb(lv_event_t *e)
{
  dm_page_t p = (dm_page_t)(uintptr_t)lv_event_get_user_data(e);
  dm_show(p);
}

void dm_create_features(void)
{
  lv_obj_t *page = lv_obj_create(g_dm_root);
  lv_obj_t *title;
  lv_obj_t *sc;
  lv_obj_t *c1;
  lv_obj_t *c2;
  lv_obj_t *c3;
  lv_obj_t *name;
  lv_obj_t *sub;
  lv_obj_t *arrow;

  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_FEATURES] = page;

  title = dm_lbl(page, "功能", "Features", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 12, 8);

  /* scroll area above dock (dock 46px at bottom) */
  sc = lv_obj_create(page);
  lv_obj_set_size(sc, DM_SCR_W, 148);
  lv_obj_set_pos(sc, 0, 28);
  lv_obj_set_style_bg_opa(sc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(sc, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_row(sc, 8, LV_PART_MAIN);
  lv_obj_set_scroll_dir(sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(sc, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(sc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  c1 = lv_obj_create(sc);
  lv_obj_set_size(c1, 292, 56);
  lv_obj_set_style_bg_color(c1, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c1, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(c1, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(c1, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c1, 10, LV_PART_MAIN);
  lv_obj_clear_flag(c1, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(c1, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c1, fn_row_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)PAGE_SUPERVISE);
  name = dm_lbl(c1, "监督", "Supervise", g_dm_font_s, C_INK);
  lv_obj_set_pos(name, 0, 2);
  sub = dm_lbl(c1, "盯着你完成一件事", "Watch one task", g_dm_font_s,
               C_MUTED);
  lv_obj_set_pos(sub, 0, 22);
  arrow = dm_lbl(c1, "›", "›", g_dm_font_m, C_MUTED);
  lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -4, 0);

  c2 = lv_obj_create(sc);
  lv_obj_set_size(c2, 292, 56);
  lv_obj_set_style_bg_color(c2, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c2, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(c2, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(c2, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c2, 10, LV_PART_MAIN);
  lv_obj_clear_flag(c2, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(c2, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c2, fn_row_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)PAGE_NOTE);
  name = dm_lbl(c2, "备忘", "Notes", g_dm_font_s, C_INK);
  lv_obj_set_pos(name, 0, 2);
  sub = dm_lbl(c2, "帮你记一下东西", "Remember things", g_dm_font_s,
               C_MUTED);
  lv_obj_set_pos(sub, 0, 22);
  arrow = dm_lbl(c2, "›", "›", g_dm_font_m, C_MUTED);
  lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -4, 0);

  c3 = lv_obj_create(sc);
  lv_obj_set_size(c3, 292, 48);
  lv_obj_set_style_bg_color(c3, lv_color_hex(0x0a0a0a), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c3, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(c3, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(c3, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c3, 10, LV_PART_MAIN);
  lv_obj_clear_flag(c3, LV_OBJ_FLAG_SCROLLABLE);
  name = dm_lbl(c3, "更多能力后续扩展", "More later", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(name, 0, 12);
}

static void dock_cb(lv_event_t *e)
{
  dm_page_t p = (dm_page_t)(uintptr_t)lv_event_get_user_data(e);
  dm_show(p);
}

void dm_dock_highlight(dm_page_t p)
{
  int i;
  dm_page_t focus_key = p;

  if (p == PAGE_FOCUS_RUN)
    {
      focus_key = PAGE_FOCUS_HOME;
    }
  else if (p == PAGE_MOOD_LOG)
    {
      focus_key = PAGE_HEALTH;
    }
  else if (p == PAGE_SUPERVISE || p == PAGE_NOTE)
    {
      focus_key = PAGE_FEATURES;
    }
  else if (p == PAGE_WIFI || p == PAGE_WIFI_PW || p == PAGE_WIFI_CONN ||
           p == PAGE_WIFI_DONE)
    {
      focus_key = PAGE_SETTINGS;
    }

  for (i = 0; i < 5; i++)
    {
      lv_obj_t *lab;
      if (!s_dock_btns[i])
        {
          continue;
        }
      lv_obj_set_style_bg_color(
          s_dock_btns[i],
          lv_color_hex(s_dock_target[i] == focus_key ? C_FACE : C_BTN),
          LV_PART_MAIN);
      lab = lv_obj_get_child(s_dock_btns[i], 0);
      if (lab)
        {
          lv_obj_set_style_text_color(
              lab,
              lv_color_hex(s_dock_target[i] == focus_key ? C_EYE : C_MUTED),
              LV_PART_MAIN);
        }
    }
}

void dm_create_dock(void)
{
  static const char *zh[] = { "专注", "聊天", "健康", "功能", "设定" };
  static const char *en[] = { "Focus", "Chat", "Health", "Features",
                              "Settings" };
  lv_obj_t *bar;
  int i;
  int w = 56;
  int gap = 6;
  int total = 5 * w + 4 * gap + 16;
  int x0 = (DM_SCR_W - total) / 2;
  if (x0 < 4)
    {
      x0 = 4;
    }

  bar = lv_obj_create(g_dm_root);
  lv_obj_set_size(bar, DM_SCR_W, 46);
  lv_obj_set_pos(bar, 0, DM_SCR_H - 46);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x080808), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_dock = bar;

  for (i = 0; i < 5; i++)
    {
      s_dock_btns[i] = dm_btn(bar, zh[i], en[i], w, 32, C_BTN, C_MUTED,
                              dock_cb, (void *)(uintptr_t)s_dock_target[i]);
      lv_obj_set_pos(s_dock_btns[i], x0 + i * (w + gap), 7);
    }

  dm_dock_highlight(PAGE_FOCUS_HOME);
}

#endif /* CONFIG_DESKMATE_APP */
