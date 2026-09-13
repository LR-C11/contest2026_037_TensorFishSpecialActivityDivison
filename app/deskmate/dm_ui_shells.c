/****************************************************************************
 * dm_ui_shells.c — skeleton pages + bottom dock
 ****************************************************************************/

#include "deskmate.h"

#ifdef CONFIG_DESKMATE_APP

static lv_obj_t *s_dock_btns[5];
static const dm_page_t s_dock_target[5] = {
  PAGE_FOCUS_HOME, PAGE_CHAT, PAGE_HEALTH, PAGE_SUPERVISE, PAGE_NOTE
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

void dm_create_supervise(void)
{
  mk_shell(PAGE_SUPERVISE, "监督", "Supervise",
           "叫我盯着你完成一件事，\n尽量不被别的打扰。",
           "Ask me to watch one task with you.");
}

void dm_create_note(void)
{
  mk_shell(PAGE_NOTE, "备忘", "Notes",
           "帮你记一下东西。\n列表与持久化骨架已就绪。",
           "I'll remember things for you.");
}

static void dock_cb(lv_event_t *e)
{
  dm_page_t p = (dm_page_t)(uintptr_t)lv_event_get_user_data(e);
  dm_show(p);
}

void dm_dock_highlight(dm_page_t p)
{
  int i;
  dm_page_t focus_key = (p == PAGE_FOCUS_RUN) ? PAGE_FOCUS_HOME : p;

  for (i = 0; i < 5; i++)
    {
      if (!s_dock_btns[i])
        {
          continue;
        }
      if (s_dock_target[i] == focus_key)
        {
          lv_obj_set_style_bg_color(s_dock_btns[i], lv_color_hex(C_FACE),
                                    LV_PART_MAIN);
          lv_obj_set_style_text_color(lv_obj_get_child(s_dock_btns[i], 0),
                                      lv_color_hex(C_EYE), LV_PART_MAIN);
        }
      else
        {
          lv_obj_set_style_bg_color(s_dock_btns[i], lv_color_hex(C_BTN),
                                    LV_PART_MAIN);
          lv_obj_set_style_text_color(lv_obj_get_child(s_dock_btns[i], 0),
                                      lv_color_hex(C_MUTED), LV_PART_MAIN);
        }
    }
}

void dm_create_dock(void)
{
  static const char *zh[] = { "专注", "聊天", "健康", "监督", "备忘" };
  static const char *en[] = { "Focus", "Chat", "Health", "Supervise",
                              "Notes" };
  lv_obj_t *dock = lv_obj_create(g_dm_root);
  int i;
  int w = 56;
  int gap = 6;
  int total = 5 * w + 4 * gap;
  int x = (DM_SCR_W - total) / 2;

  lv_obj_set_size(dock, DM_SCR_W, 46);
  lv_obj_set_pos(dock, 0, DM_SCR_H - 46);
  lv_obj_set_style_bg_color(dock, lv_color_hex(0x080808), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(dock, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(dock, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(dock, 0, LV_PART_MAIN);
  lv_obj_clear_flag(dock, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_dock = dock;

  for (i = 0; i < 5; i++)
    {
      s_dock_btns[i] =
          dm_btn(dock, zh[i], en[i], w, 32, C_BTN, C_MUTED, dock_cb,
                 (void *)(uintptr_t)s_dock_target[i]);
      lv_obj_set_pos(s_dock_btns[i], x, 7);
      x += w + gap;
    }

  dm_dock_highlight(PAGE_FOCUS_HOME);
}

#endif /* CONFIG_DESKMATE_APP */
