/****************************************************************************
 * dm_ui_med.c — 吃药提醒
 *
 * List page: large scrollable med list + bottom actions.
 * Add page: scrollable sections — presets first, then times/hours,
 *           optional larger ABC keyboard (custom name).
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#ifdef CONFIG_DESKMATE_APP

#define MED_PATH "/data/deskmate_med.txt"
#define MED_MAX 8
#define MED_NAME_MAX 16

typedef struct
{
  char name[MED_NAME_MAX];
  uint8_t times;
  uint8_t hours[3];
  uint8_t taken;
  uint16_t day;
} med_item_t;

static med_item_t s_med[MED_MAX];
static int s_med_n;
static int s_med_sel;

static char s_add_name[MED_NAME_MAX];
static int s_add_name_len;
static int s_add_times = 1;
static int s_add_h0 = 8;
static int s_add_h1 = 12;
static int s_add_h2 = 20;
static int s_add_use_kb;
static int s_add_preset = -1;

static lv_obj_t *s_med_rows[MED_MAX];
static lv_obj_t *s_med_names[MED_MAX];
static lv_obj_t *s_med_stat[MED_MAX];
static lv_obj_t *s_med_tip;
static lv_obj_t *s_med_list;

static lv_obj_t *s_add_sc;
static lv_obj_t *s_add_name_l;
static lv_obj_t *s_add_times_l;
static lv_obj_t *s_add_hours_l;
static lv_obj_t *s_add_kb_box;
static lv_obj_t *s_add_preset_btns[8];

static const char *const s_med_presets[8] = {
  "感冒药", "维生素", "胃药", "降压药",
  "消炎药", "钙片", "眼药水", "其他",
};

static void med_back(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_FEATURES);
}

static lv_obj_t *mk_med_page(dm_page_t id, const char *zh, const char *en)
{
  lv_obj_t *page = lv_obj_create(g_dm_root);
  lv_obj_t *title;
  lv_obj_t *back;

  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

  back = dm_btn(page, "←", "<", 36, 24, C_BTN, C_MUTED, med_back, NULL);
  lv_obj_set_pos(back, 8, 6);

  title = dm_lbl(page, zh, en, g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);

  g_dm_pages[id] = page;
  return page;
}

static int med_today(void)
{
  time_t t = time(NULL);
  struct tm tmv;
  localtime_r(&t, &tmv);
  return (tmv.tm_yday + tmv.tm_year * 1000) & 0xffff;
}

static void med_save(void)
{
  FILE *f = fopen(MED_PATH, "w");
  int i;
  if (!f)
    {
      return;
    }
  for (i = 0; i < s_med_n; i++)
    {
      fprintf(f, "%s|%d|%d,%d,%d|%d|%d\n", s_med[i].name, s_med[i].times,
              s_med[i].hours[0], s_med[i].hours[1], s_med[i].hours[2],
              s_med[i].taken, s_med[i].day);
    }
  fclose(f);
}

static void med_load(void)
{
  FILE *f = fopen(MED_PATH, "r");
  char line[80];
  int today = med_today();

  s_med_n = 0;
  s_med_sel = 0;
  if (!f)
    {
      return;
    }
  while (s_med_n < MED_MAX && fgets(line, sizeof(line), f))
    {
      med_item_t *m = &s_med[s_med_n];
      int t;
      int h0;
      int h1;
      int h2;
      int taken;
      int day;
      char name[MED_NAME_MAX];

      if (sscanf(line, "%15[^|]|%d|%d,%d,%d|%d|%d", name, &t, &h0, &h1,
                 &h2, &taken, &day) != 7)
        {
          continue;
        }
      snprintf(m->name, MED_NAME_MAX, "%s", name);
      m->times = (uint8_t)(t > 3 ? 3 : (t < 1 ? 1 : t));
      m->hours[0] = (uint8_t)h0;
      m->hours[1] = (uint8_t)h1;
      m->hours[2] = (uint8_t)h2;
      m->taken = (uint8_t)taken;
      m->day = (uint16_t)day;
      if (day != today)
        {
          m->taken = 0;
          m->day = (uint16_t)today;
        }
      s_med_n++;
    }
  fclose(f);
}

static void med_paint(void)
{
  int i;
  time_t now = time(NULL);
  struct tm tmv;
  int hour;

  localtime_r(&now, &tmv);
  hour = tmv.tm_hour;

  for (i = 0; i < MED_MAX; i++)
    {
      if (!s_med_rows[i])
        {
          continue;
        }
      if (i < s_med_n)
        {
          med_item_t *m = &s_med[i];
          char st[48];
          int done = 0;
          int due = 0;
          int k;

          for (k = 0; k < m->times; k++)
            {
              if (m->taken & (1u << k))
                {
                  done++;
                }
              else if (hour >= m->hours[k])
                {
                  due = 1;
                }
            }

          lv_obj_clear_flag(s_med_rows[i], LV_OBJ_FLAG_HIDDEN);
          lv_obj_set_style_bg_color(
              s_med_rows[i],
              lv_color_hex(i == s_med_sel ? 0x0d3a4a : C_BTN), LV_PART_MAIN);
          if (s_med_names[i])
            {
              lv_label_set_text(s_med_names[i], m->name);
            }
          if (s_med_stat[i])
            {
              if (done >= m->times)
                {
                  snprintf(st, sizeof(st), "已服 %d/%d", done, m->times);
                  lv_label_set_text(s_med_stat[i], st);
                  lv_obj_set_style_text_color(s_med_stat[i],
                                              lv_color_hex(C_OK),
                                              LV_PART_MAIN);
                }
              else if (due)
                {
                  snprintf(st, sizeof(st), "待服 %d/%d", done, m->times);
                  lv_label_set_text(s_med_stat[i], st);
                  lv_obj_set_style_text_color(s_med_stat[i],
                                              lv_color_hex(C_STAR),
                                              LV_PART_MAIN);
                }
              else
                {
                  size_t n;
                  snprintf(st, sizeof(st), "%d/%d ", done, m->times);
                  for (k = 0; k < m->times; k++)
                    {
                      n = strlen(st);
                      snprintf(st + n, sizeof(st) - n, "%02d ",
                               m->hours[k]);
                    }
                  lv_label_set_text(s_med_stat[i], st);
                  lv_obj_set_style_text_color(s_med_stat[i],
                                              lv_color_hex(C_MUTED),
                                              LV_PART_MAIN);
                }
            }
        }
      else
        {
          lv_obj_add_flag(s_med_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

  if (s_add_name_l)
    {
      lv_label_set_text(s_add_name_l,
                        s_add_name_len ? s_add_name : "未选择");
    }
  if (s_add_times_l)
    {
      char b[16];
      snprintf(b, sizeof(b), "%d 次 / 天", s_add_times);
      lv_label_set_text(s_add_times_l, b);
    }
  if (s_add_hours_l)
    {
      char b[40];
      if (s_add_times == 1)
        {
          snprintf(b, sizeof(b), "每天 %02d:00", s_add_h0);
        }
      else if (s_add_times == 2)
        {
          snprintf(b, sizeof(b), "%02d:00 · %02d:00", s_add_h0, s_add_h1);
        }
      else
        {
          snprintf(b, sizeof(b), "%02d · %02d · %02d", s_add_h0, s_add_h1,
                   s_add_h2);
        }
      lv_label_set_text(s_add_hours_l, b);
    }
  if (s_add_kb_box)
    {
      if (s_add_use_kb)
        {
          lv_obj_clear_flag(s_add_kb_box, LV_OBJ_FLAG_HIDDEN);
        }
      else
        {
          lv_obj_add_flag(s_add_kb_box, LV_OBJ_FLAG_HIDDEN);
        }
    }
  for (i = 0; i < 8; i++)
    {
      if (!s_add_preset_btns[i])
        {
          continue;
        }
      lv_obj_set_style_bg_color(
          s_add_preset_btns[i],
          lv_color_hex(s_add_preset == i ? 0x0d3a4a : C_BTN_HI),
          LV_PART_MAIN);
    }
}

/* ---- list actions ---- */

static void med_row_cb(lv_event_t *e)
{
  s_med_sel = (int)(intptr_t)lv_event_get_user_data(e);
  med_paint();
}

static void med_take_cb(lv_event_t *e)
{
  med_item_t *m;
  int k;
  (void)e;

  if (s_med_sel < 0 || s_med_sel >= s_med_n)
    {
      if (s_med_tip)
        {
          lv_label_set_text(s_med_tip, "先选中一种药");
        }
      return;
    }
  m = &s_med[s_med_sel];
  for (k = 0; k < m->times; k++)
    {
      if ((m->taken & (1u << k)) == 0)
        {
          m->taken |= (1u << k);
          m->day = (uint16_t)med_today();
          break;
        }
    }
  if (s_med_tip)
    {
      lv_label_set_text(s_med_tip, "已记录服药");
    }
  med_save();
  med_paint();
}

static void med_del_cb(lv_event_t *e)
{
  int i;
  (void)e;
  if (s_med_n <= 0)
    {
      return;
    }
  for (i = s_med_sel; i < s_med_n - 1; i++)
    {
      s_med[i] = s_med[i + 1];
    }
  s_med_n--;
  if (s_med_sel >= s_med_n)
    {
      s_med_sel = s_med_n > 0 ? s_med_n - 1 : 0;
    }
  if (s_med_tip)
    {
      lv_label_set_text(s_med_tip, "已删除");
    }
  med_save();
  med_paint();
}

static void med_add_open_cb(lv_event_t *e)
{
  (void)e;
  s_add_name_len = 0;
  s_add_name[0] = 0;
  s_add_times = 1;
  s_add_preset = -1;
  s_add_use_kb = 0;
  if (s_add_sc)
    {
      lv_obj_scroll_to_y(s_add_sc, 0, LV_ANIM_OFF);
    }
  med_paint();
  dm_show(PAGE_MED_ADD);
}

static void med_add_back_cb(lv_event_t *e)
{
  (void)e;
  med_paint();
  dm_show(PAGE_MED);
}

/* ---- add page ---- */

static void med_preset_cb(lv_event_t *e)
{
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  s_add_preset = i;
  snprintf(s_add_name, MED_NAME_MAX, "%s", s_med_presets[i]);
  s_add_name_len = (int)strlen(s_add_name);
  /* 「其他」→ 打开键盘自定义；其余直接用预设名 */
  s_add_use_kb = (i == 7);
  med_paint();
}

static void med_key_cb(lv_event_t *e)
{
  const char *k = (const char *)lv_event_get_user_data(e);
  if (!k || !k[0])
    {
      return;
    }
  if (strcmp(k, "DEL") == 0)
    {
      if (s_add_name_len > 0)
        {
          s_add_name_len--;
          s_add_name[s_add_name_len] = 0;
        }
    }
  else if (s_add_name_len < MED_NAME_MAX - 1)
    {
      s_add_name[s_add_name_len++] = k[0];
      s_add_name[s_add_name_len] = 0;
    }
  s_add_preset = -1;
  med_paint();
}

static void med_kb_toggle_cb(lv_event_t *e)
{
  (void)e;
  s_add_use_kb = !s_add_use_kb;
  med_paint();
  if (s_add_sc && s_add_use_kb)
    {
      lv_obj_scroll_to_view(s_add_kb_box, LV_ANIM_ON);
    }
}

static void med_times_cb(lv_event_t *e)
{
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  s_add_times += d;
  if (s_add_times < 1)
    {
      s_add_times = 1;
    }
  if (s_add_times > 3)
    {
      s_add_times = 3;
    }
  med_paint();
}

static void med_hour_cb(lv_event_t *e)
{
  const char *k = (const char *)lv_event_get_user_data(e);
  int *slots[3] = { &s_add_h0, &s_add_h1, &s_add_h2 };
  int idx;
  int d;
  int *p;

  if (!k)
    {
      return;
    }
  idx = k[0] - '0';
  d = (k[1] == '+') ? 1 : -1;
  if (idx < 0 || idx > 2)
    {
      return;
    }
  p = slots[idx];
  *p += d;
  if (*p < 0)
    {
      *p = 23;
    }
  if (*p > 23)
    {
      *p = 0;
    }
  med_paint();
}

static void med_save_add_cb(lv_event_t *e)
{
  med_item_t *m;
  (void)e;

  if (s_add_name_len <= 0)
    {
      dm_show(PAGE_MED);
      if (s_med_tip)
        {
          lv_label_set_text(s_med_tip, "请先选药名");
        }
      return;
    }
  if (s_med_n >= MED_MAX)
    {
      dm_show(PAGE_MED);
      if (s_med_tip)
        {
          lv_label_set_text(s_med_tip, "最多 8 种");
        }
      return;
    }

  m = &s_med[s_med_n];
  memset(m, 0, sizeof(*m));
  snprintf(m->name, MED_NAME_MAX, "%s", s_add_name);
  m->times = (uint8_t)s_add_times;
  m->hours[0] = (uint8_t)s_add_h0;
  m->hours[1] = (uint8_t)s_add_h1;
  m->hours[2] = (uint8_t)s_add_h2;
  m->taken = 0;
  m->day = (uint16_t)med_today();
  s_med_sel = s_med_n;
  s_med_n++;
  med_save();
  if (s_med_tip)
    {
      lv_label_set_text(s_med_tip, "已添加");
    }
  med_paint();
  dm_show(PAGE_MED);
}

static lv_obj_t *med_sec_card(lv_obj_t *parent, int h)
{
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_set_size(c, 300, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(C_BTN), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 8, LV_PART_MAIN);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

void dm_create_med(void)
{
  lv_obj_t *page;
  lv_obj_t *sc;
  lv_obj_t *b;
  lv_obj_t *card;
  lv_obj_t *lab;
  lv_obj_t *row;
  int i;

  mkdir("/data", 0755);

  /* ========== LIST PAGE ========== */
  page = mk_med_page(PAGE_MED, "吃药", "Meds");

  /* large scroll list — user can swipe */
  s_med_list = lv_obj_create(page);
  lv_obj_set_size(s_med_list, 304, 160);
  lv_obj_set_pos(s_med_list, 8, 34);
  lv_obj_set_style_bg_opa(s_med_list, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_med_list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_med_list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(s_med_list, 6, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_med_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_med_list, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(s_med_list, LV_FLEX_FLOW_COLUMN);

  for (i = 0; i < MED_MAX; i++)
    {
      row = lv_obj_create(s_med_list);
      lv_obj_set_size(row, 300, 44);
      lv_obj_set_style_bg_color(row, lv_color_hex(C_BTN), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_radius(row, 10, LV_PART_MAIN);
      lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(row, 4, LV_PART_MAIN);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(row, med_row_cb, LV_EVENT_CLICKED,
                          (void *)(intptr_t)i);
      s_med_names[i] = dm_lbl(row, "", "", g_dm_font_s, C_INK);
      lv_obj_set_pos(s_med_names[i], 8, 6);
      s_med_stat[i] = dm_lbl(row, "", "", g_dm_font_s, C_MUTED);
      lv_obj_set_pos(s_med_stat[i], 8, 24);
      s_med_rows[i] = row;
    }

  s_med_tip = dm_lbl(page, " ", " ", g_dm_font_s, C_STAR);
  lv_obj_align(s_med_tip, LV_ALIGN_TOP_MID, 0, 196);

  /* bottom actions — larger touch targets */
  b = dm_btn(page, "添加", "Add", 100, 32, 0x0d3a4a, C_ACCENT,
             med_add_open_cb, NULL);
  lv_obj_set_pos(b, 8, 204);
  b = dm_btn(page, "已服", "Taken", 100, 32, C_OK, C_EYE, med_take_cb,
             NULL);
  lv_obj_set_pos(b, 110, 204);
  b = dm_btn(page, "删除", "Del", 100, 32, C_BTN, C_INK, med_del_cb, NULL);
  lv_obj_set_pos(b, 212, 204);

  med_load();
  med_paint();

  /* ========== ADD PAGE (scrollable) ========== */
  page = mk_med_page(PAGE_MED_ADD, "添加药品", "Add Med");

  s_add_sc = lv_obj_create(page);
  lv_obj_set_size(s_add_sc, 304, 176);
  lv_obj_set_pos(s_add_sc, 8, 34);
  lv_obj_set_style_bg_opa(s_add_sc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_add_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_add_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(s_add_sc, 8, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_add_sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_add_sc, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(s_add_sc, LV_FLEX_FLOW_COLUMN);

  /* --- name + presets --- */
  card = med_sec_card(s_add_sc, 128);
  lab = dm_lbl(card, "药名", "Name", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(lab, 4, 0);
  s_add_name_l = dm_lbl(card, "未选择", "None", g_dm_font_m, C_INK);
  lv_obj_set_pos(s_add_name_l, 4, 18);

  for (i = 0; i < 8; i++)
    {
      int px = 4 + (i % 4) * 72;
      int py = 48 + (i / 4) * 36;
      b = dm_btn(card, s_med_presets[i], s_med_presets[i], 68, 32, C_BTN_HI,
                 C_DIM, med_preset_cb, (void *)(intptr_t)i);
      lv_obj_set_pos(b, px, py);
      s_add_preset_btns[i] = b;
    }

  b = dm_btn(s_add_sc, "自定义字母键盘", "ABC keyboard", 300, 32, C_BTN,
             C_ACCENT, med_kb_toggle_cb, NULL);
  lv_obj_set_width(b, 300);

  /* --- custom keyboard (hidden by default) --- */
  s_add_kb_box = lv_obj_create(s_add_sc);
  lv_obj_set_size(s_add_kb_box, 300, 140);
  lv_obj_set_style_bg_color(s_add_kb_box, lv_color_hex(0x0a0a0a),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_add_kb_box, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(s_add_kb_box, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_add_kb_box, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(s_add_kb_box, lv_color_hex(0x222222),
                                LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_add_kb_box, 6, LV_PART_MAIN);
  lv_obj_clear_flag(s_add_kb_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_add_kb_box, LV_OBJ_FLAG_HIDDEN);

  {
    static char pool[26][2];
    static const char letters[] = "QWERTYUIOPASDFGHJKLZXCVBNM";
    /* 3 rows: 10 / 9 / 7 letters + DEL */
    for (i = 0; i < 26; i++)
      {
        int rowi = (i < 10) ? 0 : (i < 19) ? 1 : 2;
        int coli = (rowi == 0) ? i : (rowi == 1) ? i - 10 : i - 19;
        int x0 = (rowi == 0) ? 4 : (rowi == 1) ? 18 : 32;
        int kw = 28;
        int kh = 36;

        pool[i][0] = letters[i];
        pool[i][1] = 0;
        b = dm_btn(s_add_kb_box, pool[i], pool[i], kw, kh, C_BTN_HI, C_INK,
                   med_key_cb, (void *)pool[i]);
        lv_obj_set_pos(b, x0 + coli * (kw + 2), 6 + rowi * (kh + 4));
      }
    b = dm_btn(s_add_kb_box, "删", "Del", 52, 36, 0x333333, C_HEART,
               med_key_cb, "DEL");
    lv_obj_set_pos(b, 232, 6 + 2 * 40);
  }

  /* --- times / hours --- */
  card = med_sec_card(s_add_sc, 100);
  lab = dm_lbl(card, "每天次数", "Times/day", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(lab, 4, 0);
  s_add_times_l = dm_lbl(card, "1 次 / 天", "1x", g_dm_font_m, C_INK);
  lv_obj_set_pos(s_add_times_l, 4, 20);
  b = dm_btn(card, "−", "-", 48, 36, C_BTN_HI, C_INK, med_times_cb,
             (void *)(intptr_t)-1);
  lv_obj_set_pos(b, 180, 12);
  b = dm_btn(card, "+", "+", 48, 36, C_BTN_HI, C_INK, med_times_cb,
             (void *)(intptr_t)1);
  lv_obj_set_pos(b, 236, 12);
  s_add_hours_l = dm_lbl(card, "每天 08:00", "08:00", g_dm_font_s, C_STAR);
  lv_obj_set_pos(s_add_hours_l, 4, 58);
  b = dm_btn(card, "−", "-", 36, 28, C_BTN, C_DIM, med_hour_cb, "0-");
  lv_obj_set_pos(b, 180, 54);
  b = dm_btn(card, "+", "+", 36, 28, C_BTN, C_DIM, med_hour_cb, "0+");
  lv_obj_set_pos(b, 220, 54);
  b = dm_btn(card, "M-", "M-", 36, 28, C_BTN, C_DIM, med_hour_cb, "1-");
  lv_obj_set_pos(b, 4, 54);
  b = dm_btn(card, "M+", "M+", 36, 28, C_BTN, C_DIM, med_hour_cb, "1+");
  lv_obj_set_pos(b, 44, 54);
  b = dm_btn(card, "E-", "E-", 36, 28, C_BTN, C_DIM, med_hour_cb, "2-");
  lv_obj_set_pos(b, 90, 54);
  b = dm_btn(card, "E+", "E+", 36, 28, C_BTN, C_DIM, med_hour_cb, "2+");
  lv_obj_set_pos(b, 130, 54);

  /* bottom bar on add page */
  b = dm_btn(page, "返回", "Back", 140, 32, C_BTN, C_MUTED, med_add_back_cb,
             NULL);
  lv_obj_set_pos(b, 12, 204);
  b = dm_btn(page, "保存", "Save", 140, 32, C_ACCENT, C_EYE, med_save_add_cb,
             NULL);
  lv_obj_set_pos(b, 168, 204);

  med_paint();
}

void dm_med_tick(void)
{
  static int acc;
  int today = med_today();
  int i;

  acc++;
  if (acc < 8)
    {
      return;
    }
  acc = 0;
  for (i = 0; i < s_med_n; i++)
    {
      if (s_med[i].day != (uint16_t)today)
        {
          s_med[i].taken = 0;
          s_med[i].day = (uint16_t)today;
        }
    }
  if (g_dm.page == PAGE_MED || g_dm.page == PAGE_MED_ADD)
    {
      med_paint();
    }
}

#endif /* CONFIG_DESKMATE_APP */
