/*
 * dm_ui_dict.c — Dictionary pages (BandLingo-style, scrollable)
 * PAGE_DICT / PAGE_DICT_SEARCH / PAGE_DICT_DETAIL
 */

#include "deskmate.h"
#include "dm_dict.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define DICT_RESULTS 20

static lv_obj_t *s_home;
static lv_obj_t *s_search;
static lv_obj_t *s_detail;
static lv_obj_t *s_q_lbl;
static lv_obj_t *s_res_sc;
static lv_obj_t *s_res_rows[DICT_RESULTS];
static lv_obj_t *s_res_sub[DICT_RESULTS];
static lv_obj_t *s_tip_l;
static lv_obj_t *s_daily_w;
static lv_obj_t *s_daily_d;
static lv_obj_t *s_detail_word;
static lv_obj_t *s_detail_def;
static lv_obj_t *s_detail_sc;

static char s_query[48];
static int s_q_len;
static bool s_q_upper;
static dm_dict_hit_t s_hits[DICT_RESULTS];
static int s_nhit;
static char s_detail_title[DM_DICT_WORD_MAX];
static char s_detail_body[256];

/* ---- shared ---- */

static lv_obj_t *dict_page_new(dm_page_t id)
{
  lv_obj_t *page = lv_obj_create(g_dm_root);
  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[id] = page;
  return page;
}

static void dict_back_features(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_FEATURES);
}

static void dict_back_home(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_DICT);
}

static void dict_back_search(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_DICT_SEARCH);
}

/* ---- detail page ---- */

static void open_detail(const char *word, const char *def)
{
  strncpy(s_detail_title, word, sizeof(s_detail_title) - 1);
  s_detail_title[sizeof(s_detail_title) - 1] = '\0';
  if (def && def[0])
    {
      strncpy(s_detail_body, def, sizeof(s_detail_body) - 1);
      s_detail_body[sizeof(s_detail_body) - 1] = '\0';
    }
  else
    {
      s_detail_body[0] = '\0';
    }

  if (s_detail_word)
    {
      lv_label_set_text(s_detail_word, s_detail_title);
    }
  if (s_detail_def)
    {
      if (s_detail_body[0])
        {
          lv_label_set_text(s_detail_def, s_detail_body);
        }
      else
        {
          char tmp[256];
          if (dm_dict_lookup(s_detail_title, tmp, sizeof(tmp)) == 0)
            {
              strncpy(s_detail_body, tmp, sizeof(s_detail_body) - 1);
              lv_label_set_text(s_detail_def, s_detail_body);
            }
          else
            {
              lv_label_set_text(s_detail_def, dm_t("暂无释义", "No definition"));
            }
        }
    }
  if (s_detail_sc)
    {
      lv_obj_scroll_to_y(s_detail_sc, 0, LV_ANIM_OFF);
    }
  dm_show(PAGE_DICT_DETAIL);
}

static void detail_row_click(lv_event_t *e)
{
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx >= 0 && idx < s_nhit)
    {
      open_detail(s_hits[idx].word, s_hits[idx].def);
    }
}

/* ---- search page ---- */

static void refresh_results(void)
{
  int i;
  for (i = 0; i < DICT_RESULTS; i++)
    {
      if (!s_res_rows[i])
        {
          continue;
        }
      if (i < s_nhit)
        {
          char line[72];
          lv_obj_clear_flag(s_res_rows[i], LV_OBJ_FLAG_HIDDEN);
          snprintf(line, sizeof(line), "%s", s_hits[i].word);
          lv_label_set_text(lv_obj_get_child(s_res_rows[i], 0), line);
          if (s_res_sub[i])
            {
              lv_label_set_text(s_res_sub[i], s_hits[i].def);
            }
        }
      else
        {
          lv_obj_add_flag(s_res_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
  if (s_res_sc)
    {
      lv_obj_scroll_to_y(s_res_sc, 0, LV_ANIM_OFF);
    }
}

static void do_search(void)
{
  if (s_q_len <= 0)
    {
      s_nhit = 0;
      if (s_tip_l)
        {
          lv_label_set_text(s_tip_l, dm_t("输入英文或中文释义", "Type EN or CN"));
        }
      refresh_results();
      return;
    }

  {
    bool chinese = false;
    int i;
    for (i = 0; s_query[i]; i++)
      {
        if ((unsigned char)s_query[i] > 0x7f)
          {
            chinese = true;
            break;
          }
      }
    s_nhit = dm_dict_search(s_query, s_hits, DICT_RESULTS, chinese);
  }

  if (s_tip_l)
    {
      char msg[48];
      if (s_nhit > 0)
        {
          snprintf(msg, sizeof(msg), dm_t("找到 %d 条，点选查看", " %d hits"),
                   s_nhit);
        }
      else
        {
          snprintf(msg, sizeof(msg), dm_t("没有找到，换个词试试", "No match"));
        }
      lv_label_set_text(s_tip_l, msg);
    }
  refresh_results();
}

static void q_refresh(void)
{
  if (s_q_lbl)
    {
      char show[56];
      if (s_q_len == 0)
        {
          snprintf(show, sizeof(show), "%s", dm_t("输入单词…", "Type word…"));
        }
      else
        {
          snprintf(show, sizeof(show), "%s", s_query);
        }
      lv_label_set_text(s_q_lbl, show);
    }
}

static void kb_add(char c)
{
  if (s_q_len >= (int)sizeof(s_query) - 1)
    {
      return;
    }
  if (s_q_upper && c >= 'a' && c <= 'z')
    {
      c = (char)(c - 'a' + 'A');
    }
  s_query[s_q_len++] = c;
  s_query[s_q_len] = '\0';
  q_refresh();
}

static void kb_key_cb(lv_event_t *e)
{
  kb_add((char)(uintptr_t)lv_event_get_user_data(e));
}

static void kb_del_cb(lv_event_t *e)
{
  (void)e;
  if (s_q_len > 0)
    {
      s_query[--s_q_len] = '\0';
      q_refresh();
    }
}

static void kb_clear_cb(lv_event_t *e)
{
  (void)e;
  s_q_len = 0;
  s_query[0] = '\0';
  q_refresh();
}

static void kb_case_cb(lv_event_t *e)
{
  (void)e;
  s_q_upper = !s_q_upper;
}

static void kb_go_cb(lv_event_t *e)
{
  (void)e;
  do_search();
}

static void dict_open_search(lv_event_t *e)
{
  (void)e;
  s_q_len = 0;
  s_query[0] = '\0';
  q_refresh();
  do_search();
  dm_show(PAGE_DICT_SEARCH);
}

static void dict_ai_cb(lv_event_t *e)
{
  (void)e;
  if (s_q_len <= 0)
    {
      if (s_tip_l)
        {
          lv_label_set_text(s_tip_l, dm_t("先输入单词", "Type a word first"));
        }
      return;
    }
  if (s_tip_l)
    {
      lv_label_set_text(s_tip_l, dm_t("AI 查询中…", "AI querying…"));
    }
  {
    char def[200];
    if (dm_dict_ai_define(s_query, def, sizeof(def)) == 0)
      {
        open_detail(s_query, def);
        if (s_tip_l)
          {
            lv_label_set_text(s_tip_l, dm_t("AI 查询完成", "AI done"));
          }
      }
    else if (s_tip_l)
      {
        lv_label_set_text(s_tip_l, dm_t("AI 不可用（需联网）", "AI needs network"));
      }
  }
}

/* ---- home page ---- */

void dm_create_dict(void)
{
  lv_obj_t *title, *back, *btn_search, *c_daily, *lab;

  /* ===== HOME ===== */
  s_home = dict_page_new(PAGE_DICT);
  back = dm_btn(s_home, "←", "<", 36, 24, C_BTN, C_MUTED, dict_back_features, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_home, "词典", "Dictionary", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);

  btn_search = dm_btn(s_home, "查词", "Search", 80, 28, C_BTN_HI, C_ACCENT,
                      dict_open_search, NULL);
  lv_obj_set_pos(btn_search, 232, 6);

  c_daily = lv_obj_create(s_home);
  lv_obj_set_size(c_daily, 304, 96);
  lv_obj_set_pos(c_daily, 8, 42);
  lv_obj_set_style_bg_color(c_daily, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c_daily, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(c_daily, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(c_daily, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c_daily, 10, LV_PART_MAIN);
  lv_obj_clear_flag(c_daily, LV_OBJ_FLAG_SCROLLABLE);

  lab = dm_lbl(c_daily, "每日一词", "Word of the day", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(lab, 0, 0);
  s_daily_w = dm_lbl(c_daily, "—", "—", g_dm_font_m, C_ACCENT);
  lv_obj_set_pos(s_daily_w, 0, 22);
  s_daily_d = dm_lbl(c_daily, "", "", g_dm_font_s, C_INK);
  lv_label_set_long_mode(s_daily_d, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_daily_d, 280);
  lv_obj_set_pos(s_daily_d, 0, 48);

  {
    lv_obj_t *tip = dm_lbl(s_home,
                           "本地词库 6.5 万词 · 支持中英互查",
                           "65k words · EN/CN lookup",
                           g_dm_font_s, C_DIM);
    lv_obj_set_pos(tip, 12, 160);
    lv_obj_t *tip2 = dm_lbl(s_home,
                            "左右/上下滑动浏览结果",
                            "Swipe to browse results",
                            g_dm_font_s, C_DIM);
    lv_obj_set_pos(tip2, 12, 182);
    lv_obj_t *tip3 = dm_lbl(s_home,
                            "词库文件: /resource/dict/ 或 /data/dict/",
                            "Dict files under /resource/dict/",
                            g_dm_font_s, C_MUTED);
    lv_obj_set_pos(tip3, 12, 204);
  }

  /* ===== SEARCH ===== */
  s_search = dict_page_new(PAGE_DICT_SEARCH);
  back = dm_btn(s_search, "←", "<", 36, 24, C_BTN, C_MUTED, dict_back_home, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_search, "查词", "Search", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);
  {
    lv_obj_t *ai = dm_btn(s_search, "AI", "AI", 48, 24, C_BTN_HI, C_STAR,
                          dict_ai_cb, NULL);
    lv_obj_set_pos(ai, 264, 6);
  }

  s_q_lbl = dm_lbl(s_search, "", "", g_dm_font_m, C_ACCENT);
  lv_obj_set_pos(s_q_lbl, 12, 36);

  s_tip_l = dm_lbl(s_search, "输入英文或中文释义", "Type EN or CN",
                   g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_tip_l, 12, 58);

  /* results scrollable */
  {
    lv_obj_t *box = lv_obj_create(s_search);
    lv_obj_set_size(box, 304, 72);
    lv_obj_set_pos(box, 8, 76);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x0c0c0c), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(box, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 4, LV_PART_MAIN);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    s_res_sc = lv_obj_create(box);
    lv_obj_set_size(s_res_sc, 292, 64);
    lv_obj_set_pos(s_res_sc, 0, 0);
    lv_obj_set_style_bg_opa(s_res_sc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_res_sc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_res_sc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_res_sc, 3, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_res_sc, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_res_sc, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(s_res_sc, LV_FLEX_FLOW_COLUMN);

    {
      int i;
      for (i = 0; i < DICT_RESULTS; i++)
        {
          lv_obj_t *row = lv_obj_create(s_res_sc);
          lv_obj_set_size(row, 284, 36);
          lv_obj_set_style_bg_color(row, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
          lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
          lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
          lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
          lv_obj_set_style_pad_all(row, 4, LV_PART_MAIN);
          lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
          lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
          lv_obj_add_event_cb(row, detail_row_click, LV_EVENT_CLICKED,
                              (void *)(intptr_t)i);
          {
            lv_obj_t *w = dm_lbl(row, "", "", g_dm_font_s, C_INK);
            lv_obj_set_pos(w, 4, 2);
            lv_obj_t *d = dm_lbl(row, "", "", g_dm_font_s, C_MUTED);
            lv_label_set_long_mode(d, LV_LABEL_LONG_DOT);
            lv_obj_set_width(d, 260);
            lv_obj_set_pos(d, 4, 18);
            s_res_sub[i] = d;
          }
          s_res_rows[i] = row;
          lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        }
    }
  }

  /* compact keyboard */
  {
    static const char *rows[] = {
      "qwertyuiop",
      "asdfghjkl",
      "zxcvbnm",
    };
    int r;
    for (r = 0; r < 3; r++)
      {
        int c;
        int n = (int)strlen(rows[r]);
        int key_w = 28;
        int gap = 3;
        int x0 = (DM_SCR_W - (n * key_w + (n - 1) * gap)) / 2;
        for (c = 0; c < n; c++)
          {
            char t[2] = { rows[r][c], 0 };
            lv_obj_t *b = dm_btn(s_search, t, t, key_w, 22, 0x222222, C_INK,
                                 kb_key_cb, (void *)(uintptr_t)(unsigned char)rows[r][c]);
            lv_obj_set_style_radius(b, 6, LV_PART_MAIN);
            lv_obj_set_pos(b, x0 + c * (key_w + gap), 154 + r * 20);
          }
      }
    {
      lv_obj_t *b;
      b = dm_btn(s_search, "空格", "SP", 70, 22, 0x222222, C_INK, kb_key_cb,
                 (void *)(uintptr_t)' ');
      lv_obj_set_pos(b, 8, 214);
      b = dm_btn(s_search, "删", "Del", 48, 22, 0x333333, C_MUTED, kb_del_cb, NULL);
      lv_obj_set_pos(b, 84, 214);
      b = dm_btn(s_search, "清", "Clr", 48, 22, 0x333333, C_MUTED, kb_clear_cb, NULL);
      lv_obj_set_pos(b, 138, 214);
      b = dm_btn(s_search, "A/a", "Aa", 48, 22, 0x333333, C_MUTED, kb_case_cb, NULL);
      lv_obj_set_pos(b, 192, 214);
      b = dm_btn(s_search, "搜索", "Go", 56, 22, C_BTN_HI, C_ACCENT, kb_go_cb, NULL);
      lv_obj_set_pos(b, 246, 214);
    }
  }

  /* ===== DETAIL ===== */
  s_detail = dict_page_new(PAGE_DICT_DETAIL);
  back = dm_btn(s_detail, "←", "<", 36, 24, C_BTN, C_MUTED, dict_back_search, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_detail, "释义", "Detail", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);

  s_detail_word = dm_lbl(s_detail, "", "", g_dm_font_l, C_ACCENT);
  lv_obj_set_pos(s_detail_word, 12, 40);

  {
    lv_obj_t *box = lv_obj_create(s_detail);
    lv_obj_set_size(box, 304, 150);
    lv_obj_set_pos(box, 8, 72);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(box, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 10, LV_PART_MAIN);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    s_detail_sc = lv_obj_create(box);
    lv_obj_set_size(s_detail_sc, 284, 130);
    lv_obj_set_pos(s_detail_sc, 0, 0);
    lv_obj_set_style_bg_opa(s_detail_sc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_detail_sc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_detail_sc, 0, LV_PART_MAIN);
    lv_obj_set_scroll_dir(s_detail_sc, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_detail_sc, LV_SCROLLBAR_MODE_AUTO);

    s_detail_def = dm_lbl(s_detail_sc, "", "", g_dm_font_m, C_INK);
    lv_label_set_long_mode(s_detail_def, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_detail_def, 270);
    lv_obj_set_pos(s_detail_def, 0, 0);
  }

  /* init data */
  dm_dict_init();
  {
    char def[128];
    const char *w = dm_dict_daily_word(def, sizeof(def));
    if (w && w[0])
      {
        lv_label_set_text(s_daily_w, w);
        lv_label_set_text(s_daily_d, def);
      }
    else
      {
        lv_label_set_text(s_daily_w, "apple");
        lv_label_set_text(s_daily_d, "苹果；苹果树");
      }
  }
}

#endif /* CONFIG_DESKMATE_APP */
