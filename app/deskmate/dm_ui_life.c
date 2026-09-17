/****************************************************************************
 * dm_ui_life.c — Water reminder + Countdown days (file persistence)
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef CONFIG_DESKMATE_APP

#define WATER_PATH "/data/deskmate_water.txt"
#define COUNTDOWN_PATH "/data/deskmate_countdown.txt"
#define CD_MAX 8
#define CD_NAME_MAX 16

static void life_back(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_FEATURES);
}

static lv_obj_t *mk_life_page(dm_page_t id, const char *zh, const char *en)
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

  back = dm_btn(page, "←", "<", 36, 24, C_BTN, C_MUTED, life_back, NULL);
  lv_obj_set_pos(back, 8, 6);

  title = dm_lbl(page, zh, en, g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);

  g_dm_pages[id] = page;
  return page;
}

/* ---------- water ---------- */

#define WATER_GOAL 8

static int s_w_cups;
static int s_w_interval = 45;
static int s_w_left;
static int s_w_day;
static time_t s_w_due; /* absolute next reminder time */
static lv_obj_t *s_w_big;
static lv_obj_t *s_w_next;
static lv_obj_t *s_w_tip;
static lv_obj_t *s_w_chips[4];

static int today_yday(void)
{
  time_t t = time(NULL);
  struct tm tmv;
  localtime_r(&t, &tmv);
  return tmv.tm_yday + tmv.tm_year * 1000;
}

static void water_save(void)
{
  FILE *f;
  s_w_due = time(NULL) + s_w_left;
  f = fopen(WATER_PATH, "w");
  if (!f)
    {
      return;
    }
  fprintf(f, "%d %d %ld %d\n", s_w_cups, s_w_interval, (long)s_w_due,
          s_w_day);
  fclose(f);
}

static void water_load(void)
{
  FILE *f = fopen(WATER_PATH, "r");
  int c = 0;
  int iv = 45;
  long due = 0;
  int day = today_yday();
  time_t now = time(NULL);

  if (f)
    {
      if (fscanf(f, "%d %d %ld %d", &c, &iv, &due, &day) != 4)
        {
          day = today_yday();
          due = 0;
        }
      fclose(f);
    }
  if (iv != 30 && iv != 45 && iv != 60 && iv != 90)
    {
      iv = 45;
    }
  if (day != today_yday())
    {
      c = 0;
      due = (long)now + iv * 60;
    }
  if (due <= (long)now)
    {
      due = (long)now + iv * 60;
    }
  s_w_cups = c;
  s_w_interval = iv;
  s_w_due = (time_t)due;
  s_w_left = (int)(s_w_due - now);
  if (s_w_left < 0)
    {
      s_w_left = 0;
    }
  s_w_day = today_yday();
}

static void water_paint(void)
{
  char buf[32];
  int m;
  int s;
  int i;

  if (s_w_big)
    {
      snprintf(buf, sizeof(buf), "%d / %d", s_w_cups, WATER_GOAL);
      lv_label_set_text(s_w_big, buf);
    }
  m = s_w_left / 60;
  s = s_w_left % 60;
  if (s_w_next)
    {
      snprintf(buf, sizeof(buf), "下次提醒 %02d:%02d", m, s);
      lv_label_set_text(s_w_next, buf);
    }
  for (i = 0; i < 4; i++)
    {
      if (!s_w_chips[i])
        {
          continue;
        }
      lv_obj_set_style_bg_color(
          s_w_chips[i],
          lv_color_hex((i == 0 && s_w_interval == 30) ||
                               (i == 1 && s_w_interval == 45) ||
                               (i == 2 && s_w_interval == 60) ||
                               (i == 3 && s_w_interval == 90)
                           ? C_STAR
                           : C_BTN),
          LV_PART_MAIN);
      lv_obj_set_style_text_color(
          lv_obj_get_child(s_w_chips[i], 0),
          lv_color_hex((i == 0 && s_w_interval == 30) ||
                               (i == 1 && s_w_interval == 45) ||
                               (i == 2 && s_w_interval == 60) ||
                               (i == 3 && s_w_interval == 90)
                           ? C_EYE
                           : C_DIM),
          LV_PART_MAIN);
    }
}

static void water_chip_cb(lv_event_t *e)
{
  int iv = (int)(intptr_t)lv_event_get_user_data(e);
  s_w_interval = iv;
  s_w_left = iv * 60;
  water_save();
  water_paint();
}

static void water_drink_cb(lv_event_t *e)
{
  (void)e;
  s_w_cups++;
  s_w_left = s_w_interval * 60;
  if (s_w_tip)
    {
      lv_label_set_text(s_w_tip,
                        s_w_cups >= WATER_GOAL ? "今日目标达成！"
                                               : "记下了，记得休息眼睛");
    }
  water_save();
  water_paint();
}

static void water_reset_cb(lv_event_t *e)
{
  (void)e;
  s_w_cups = 0;
  s_w_left = s_w_interval * 60;
  if (s_w_tip)
    {
      lv_label_set_text(s_w_tip, " ");
    }
  water_save();
  water_paint();
}

void dm_create_water(void)
{
  lv_obj_t *page = mk_life_page(PAGE_WATER, "喝水提醒", "Water");
  lv_obj_t *b;
  static const char *zh_iv[4] = { "30分", "45分", "60分", "90分" };
  int ivs[4] = { 30, 45, 60, 90 };
  int i;

  s_w_big = dm_lbl(page, "0 / 8", "0 / 8", g_dm_font_xl, C_ACCENT);
  lv_obj_align(s_w_big, LV_ALIGN_TOP_MID, 0, 36);

  s_w_next = dm_lbl(page, "下次提醒 45:00", "Next 45:00", g_dm_font_s,
                    C_MUTED);
  lv_obj_align(s_w_next, LV_ALIGN_TOP_MID, 0, 88);

  for (i = 0; i < 4; i++)
    {
      b = dm_btn(page, zh_iv[i], zh_iv[i], 68, 28, C_BTN, C_DIM,
                 water_chip_cb, (void *)(intptr_t)ivs[i]);
      lv_obj_set_pos(b, 8 + i * 76, 112);
      s_w_chips[i] = b;
    }

  b = dm_btn(page, "喝了一杯", "Drink", 148, 36, 0x0d3a4a, C_ACCENT,
             water_drink_cb, NULL);
  lv_obj_set_pos(b, 8, 156);
  b = dm_btn(page, "今日清零", "Reset", 148, 36, C_BTN, C_INK,
             water_reset_cb, NULL);
  lv_obj_set_pos(b, 164, 156);

  s_w_tip = dm_lbl(page, " ", " ", g_dm_font_s, C_STAR);
  lv_obj_align(s_w_tip, LV_ALIGN_TOP_MID, 0, 204);

  water_load();
  water_paint();
}

void dm_water_tick(void)
{
  time_t now = time(NULL);
  int left;

  if (s_w_day != today_yday())
    {
      s_w_cups = 0;
      s_w_day = today_yday();
      s_w_due = now + s_w_interval * 60;
      water_save();
    }

  left = (int)(s_w_due - now);
  if (left < 0)
    {
      left = 0;
    }
  if (left != s_w_left)
    {
      s_w_left = left;
      if (g_dm.page == PAGE_WATER)
        {
          if (s_w_tip && s_w_left == 0)
            {
              lv_label_set_text(s_w_tip, "该喝水啦！");
            }
          water_paint();
        }
    }
}

/* ---------- countdown ---------- */

typedef struct
{
  int y;
  int m;
  int d;
  char name[CD_NAME_MAX];
} cd_item_t;

static cd_item_t s_cd[CD_MAX];
static int s_cd_n;
static int s_cd_sel;
static bool s_cd_form;
static int s_cd_fy;
static int s_cd_fm;
static int s_cd_fd;
static int s_cd_fname; /* 0..4 preset index */
static lv_obj_t *s_cd_rows[CD_MAX];
static lv_obj_t *s_cd_days[CD_MAX];
static lv_obj_t *s_cd_names[CD_MAX];
static lv_obj_t *s_cd_form_box;
static lv_obj_t *s_cd_date_l;
static lv_obj_t *s_cd_name_btns[5];

static const char *const s_cd_presets[5] = {
  "生日", "考试", "旅行", "纪念日", "项目"
};

static void cd_save(void)
{
  FILE *f = fopen(COUNTDOWN_PATH, "w");
  int i;
  if (!f)
    {
      return;
    }
  for (i = 0; i < s_cd_n; i++)
    {
      fprintf(f, "%04d-%02d-%02d %s\n", s_cd[i].y, s_cd[i].m, s_cd[i].d,
              s_cd[i].name);
    }
  fclose(f);
}

static void cd_load(void)
{
  FILE *f = fopen(COUNTDOWN_PATH, "r");
  char line[64];
  s_cd_n = 0;
  s_cd_sel = 0;
  if (!f)
    {
      /* seed demo */
      time_t t = time(NULL);
      struct tm tmv;
      localtime_r(&t, &tmv);
      s_cd[0].y = tmv.tm_year + 1900;
      s_cd[0].m = 10;
      s_cd[0].d = 1;
      snprintf(s_cd[0].name, CD_NAME_MAX, "生日");
      s_cd[1].y = tmv.tm_year + 1900;
      s_cd[1].m = 12;
      s_cd[1].d = 20;
      snprintf(s_cd[1].name, CD_NAME_MAX, "考试");
      s_cd_n = 2;
      return;
    }
  while (s_cd_n < CD_MAX && fgets(line, sizeof(line), f))
    {
      int y;
      int m;
      int d;
      char name[CD_NAME_MAX];
      if (sscanf(line, "%d-%d-%d %15s", &y, &m, &d, name) == 4)
        {
          s_cd[s_cd_n].y = y;
          s_cd[s_cd_n].m = m;
          s_cd[s_cd_n].d = d;
          snprintf(s_cd[s_cd_n].name, CD_NAME_MAX, "%s", name);
          s_cd_n++;
        }
    }
  fclose(f);
}

static int cd_days(const cd_item_t *it)
{
  time_t now = time(NULL);
  struct tm tmv;
  struct tm t2;
  time_t target;

  localtime_r(&now, &tmv);
  memset(&t2, 0, sizeof(t2));
  t2.tm_year = it->y - 1900;
  t2.tm_mon = it->m - 1;
  t2.tm_mday = it->d;
  t2.tm_hour = 12;
  target = mktime(&t2);
  if (target == (time_t)-1)
    {
      return 0;
    }
  {
    struct tm a;
    struct tm b;
    localtime_r(&target, &a);
    localtime_r(&now, &b);
    a.tm_hour = a.tm_min = a.tm_sec = 0;
    b.tm_hour = b.tm_min = b.tm_sec = 0;
    return (int)difftime(mktime(&a), mktime(&b)) / 86400;
  }
}

static void cd_paint(void)
{
  int i;
  char buf[32];

  for (i = 0; i < CD_MAX; i++)
    {
      if (!s_cd_rows[i])
        {
          continue;
        }
      if (i < s_cd_n)
        {
          int days = cd_days(&s_cd[i]);
          lv_obj_clear_flag(s_cd_rows[i], LV_OBJ_FLAG_HIDDEN);
          lv_obj_set_style_bg_color(
              s_cd_rows[i],
              lv_color_hex(i == s_cd_sel ? 0x0d3a4a : C_BTN), LV_PART_MAIN);
          if (s_cd_names[i])
            {
              lv_label_set_text(s_cd_names[i], s_cd[i].name);
            }
          if (s_cd_days[i])
            {
              if (days < 0)
                {
                  lv_label_set_text(s_cd_days[i], "已过");
                  lv_obj_set_style_text_color(s_cd_days[i],
                                              lv_color_hex(C_MUTED),
                                              LV_PART_MAIN);
                }
              else
                {
                  snprintf(buf, sizeof(buf), "%d", days);
                  lv_label_set_text(s_cd_days[i], buf);
                  lv_obj_set_style_text_color(s_cd_days[i],
                                              lv_color_hex(C_ACCENT),
                                              LV_PART_MAIN);
                }
            }
        }
      else
        {
          lv_obj_add_flag(s_cd_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

  if (s_cd_date_l)
    {
      snprintf(buf, sizeof(buf), "%04d-%02d-%02d", s_cd_fy, s_cd_fm, s_cd_fd);
      lv_label_set_text(s_cd_date_l, buf);
    }
  if (s_cd_form_box)
    {
      if (s_cd_form)
        {
          lv_obj_clear_flag(s_cd_form_box, LV_OBJ_FLAG_HIDDEN);
        }
      else
        {
          lv_obj_add_flag(s_cd_form_box, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void cd_row_cb(lv_event_t *e)
{
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  s_cd_sel = i;
  cd_paint();
}

static void cd_add_cb(lv_event_t *e)
{
  int i;
  (void)e;
  s_cd_form = !s_cd_form;
  if (s_cd_form)
    {
      s_cd_fname = 0;
      for (i = 0; i < 5; i++)
        {
          if (s_cd_name_btns[i])
            {
              lv_obj_set_style_bg_color(s_cd_name_btns[i],
                                        lv_color_hex(i == 0 ? 0x0d3a4a
                                                            : 0x222222),
                                        LV_PART_MAIN);
            }
        }
    }
  cd_paint();
}

static void cd_del_cb(lv_event_t *e)
{
  int i;
  (void)e;
  if (s_cd_n <= 0)
    {
      return;
    }
  for (i = s_cd_sel; i < s_cd_n - 1; i++)
    {
      s_cd[i] = s_cd[i + 1];
    }
  s_cd_n--;
  if (s_cd_sel >= s_cd_n)
    {
      s_cd_sel = s_cd_n > 0 ? s_cd_n - 1 : 0;
    }
  cd_save();
  cd_paint();
}

static void cd_preset_cb(lv_event_t *e)
{
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  int j;
  s_cd_fname = i;
  for (j = 0; j < 5; j++)
    {
      if (s_cd_name_btns[j])
        {
          lv_obj_set_style_bg_color(s_cd_name_btns[j],
                                    lv_color_hex(j == i ? 0x0d3a4a
                                                        : 0x222222),
                                    LV_PART_MAIN);
        }
    }
  cd_paint();
}

static void cd_date_cb(lv_event_t *e)
{
  const char *k = (const char *)lv_event_get_user_data(e);
  if (!k)
    {
      return;
    }
  if (strcmp(k, "y-") == 0)
    {
      s_cd_fy--;
    }
  else if (strcmp(k, "y+") == 0)
    {
      s_cd_fy++;
    }
  else if (strcmp(k, "m-") == 0)
    {
      s_cd_fm = s_cd_fm <= 1 ? 12 : s_cd_fm - 1;
    }
  else if (strcmp(k, "m+") == 0)
    {
      s_cd_fm = s_cd_fm >= 12 ? 1 : s_cd_fm + 1;
    }
  else if (strcmp(k, "d-") == 0)
    {
      s_cd_fd = s_cd_fd <= 1 ? 31 : s_cd_fd - 1;
    }
  else if (strcmp(k, "d+") == 0)
    {
      s_cd_fd = s_cd_fd >= 31 ? 1 : s_cd_fd + 1;
    }
  cd_paint();
}

static void cd_save_cb(lv_event_t *e)
{
  (void)e;
  if (s_cd_n >= CD_MAX)
    {
      return;
    }
  s_cd[s_cd_n].y = s_cd_fy;
  s_cd[s_cd_n].m = s_cd_fm;
  s_cd[s_cd_n].d = s_cd_fd;
  snprintf(s_cd[s_cd_n].name, CD_NAME_MAX, "%s", s_cd_presets[s_cd_fname]);
  s_cd_sel = s_cd_n;
  s_cd_n++;
  s_cd_form = false;
  cd_save();
  cd_paint();
}

void dm_create_countdown(void)
{
  lv_obj_t *page = mk_life_page(PAGE_COUNTDOWN, "倒数日", "Countdown");
  lv_obj_t *sc;
  lv_obj_t *row;
  lv_obj_t *b;
  int i;
  time_t t = time(NULL);
  struct tm tmv;

  localtime_r(&t, &tmv);
  s_cd_fy = tmv.tm_year + 1900;
  s_cd_fm = tmv.tm_mon + 1;
  s_cd_fd = tmv.tm_mday;

  sc = lv_obj_create(page);
  lv_obj_set_size(sc, 304, 164);
  lv_obj_set_pos(sc, 8, 34);
  lv_obj_set_style_bg_opa(sc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(sc, 4, LV_PART_MAIN);
  lv_obj_set_scroll_dir(sc, LV_DIR_VER);
  lv_obj_set_flex_flow(sc, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(sc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  for (i = 0; i < CD_MAX; i++)
    {
      row = lv_obj_create(sc);
      lv_obj_set_size(row, 300, 22);
      lv_obj_set_style_bg_color(row, lv_color_hex(C_BTN), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
      lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(row, 2, LV_PART_MAIN);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(row, cd_row_cb, LV_EVENT_CLICKED,
                          (void *)(intptr_t)i);
      s_cd_names[i] = dm_lbl(row, "", "", g_dm_font_s, C_INK);
      lv_obj_set_pos(s_cd_names[i], 4, 3);
      s_cd_days[i] = dm_lbl(row, "", "", g_dm_font_s, C_ACCENT);
      lv_obj_align(s_cd_days[i], LV_ALIGN_RIGHT_MID, -4, 0);
      s_cd_rows[i] = row;
    }

  b = dm_btn(page, "添加", "Add", 140, 24, 0x0d3a4a, C_ACCENT, cd_add_cb,
             NULL);
  lv_obj_set_pos(b, 16, 206);
  b = dm_btn(page, "删除选中", "Del", 140, 24, C_BTN, C_INK, cd_del_cb,
             NULL);
  lv_obj_set_pos(b, 164, 206);

  s_cd_form_box = lv_obj_create(page);
  lv_obj_set_size(s_cd_form_box, 304, 164);
  lv_obj_set_pos(s_cd_form_box, 8, 34);
  lv_obj_set_style_bg_color(s_cd_form_box, lv_color_hex(0x111111),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_cd_form_box, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(s_cd_form_box, 10, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_cd_form_box, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_cd_form_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_cd_form_box, LV_OBJ_FLAG_HIDDEN);

  for (i = 0; i < 5; i++)
    {
      b = dm_btn(s_cd_form_box, s_cd_presets[i], s_cd_presets[i], 52, 22,
                 0x222222, C_DIM, cd_preset_cb, (void *)(intptr_t)i);
      lv_obj_set_pos(b, 8 + i * 58, 10);
      s_cd_name_btns[i] = b;
    }

  s_cd_date_l = dm_lbl(s_cd_form_box, "2026-01-01", "2026-01-01",
                       g_dm_font_m, C_INK);
  lv_obj_set_pos(s_cd_date_l, 12, 48);

  b = dm_btn(s_cd_form_box, "−", "-", 28, 22, C_BTN, C_DIM, cd_date_cb,
             "y-");
  lv_obj_set_pos(b, 12, 80);
  b = dm_btn(s_cd_form_box, "+", "+", 28, 22, C_BTN, C_DIM, cd_date_cb,
             "y+");
  lv_obj_set_pos(b, 44, 80);
  b = dm_btn(s_cd_form_box, "−月", "-M", 36, 22, C_BTN, C_DIM, cd_date_cb,
             "m-");
  lv_obj_set_pos(b, 90, 80);
  b = dm_btn(s_cd_form_box, "+月", "+M", 36, 22, C_BTN, C_DIM, cd_date_cb,
             "m+");
  lv_obj_set_pos(b, 130, 80);
  b = dm_btn(s_cd_form_box, "−日", "-D", 36, 22, C_BTN, C_DIM, cd_date_cb,
             "d-");
  lv_obj_set_pos(b, 176, 80);
  b = dm_btn(s_cd_form_box, "+日", "+D", 36, 22, C_BTN, C_DIM, cd_date_cb,
             "d+");
  lv_obj_set_pos(b, 216, 80);

  b = dm_btn(s_cd_form_box, "保存", "Save", 288, 28, C_ACCENT, C_EYE,
             cd_save_cb, NULL);
  lv_obj_set_pos(b, 8, 122);

  cd_load();
  cd_paint();
}

/* ---------- entry ---------- */

void dm_create_life(void)
{
  mkdir("/data", 0755);
  dm_create_water();
  dm_create_countdown();
}

void dm_life_tick(void)
{
  dm_water_tick();
}

#endif /* CONFIG_DESKMATE_APP */
