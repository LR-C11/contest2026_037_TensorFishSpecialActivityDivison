/****************************************************************************
 * dm_ui_health.c — health page + full-page mood log + file storage
 *
 * Flow: Health → 「记录心情」全页 → 保存写入 /data → 返回健康页刷新
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef CONFIG_DESKMATE_APP

#define DM_MOOD_PATH "/data/deskmate_mood.bin"
#define DM_MOOD_MAGIC 0x4d4f4f44u /* 'MOOD' */

#define M_NEG (DM_M_SAD | DM_M_ANGRY | DM_M_TIRED | DM_M_ANXIOUS | \
               DM_M_LONELY | DM_M_STRESS)

typedef struct
{
  const char *zh;
  const char *en;
  uint16_t bit;
  int score;
} mood_item_t;

static const mood_item_t s_moods[] = {
  { "开心", "Joy", DM_M_JOY, 85 },
  { "喜爱", "Love", DM_M_LOVE, 82 },
  { "兴奋", "Excite", DM_M_EXCITE, 90 },
  { "自信", "Confident", DM_M_CONFIDENT, 88 },
  { "期待", "Expect", DM_M_EXPECT, 80 },
  { "满足", "Satisfy", DM_M_SATISFY, 75 },
  { "感动", "Touched", DM_M_TOUCHED, 80 },
  { "平静", "Calm", DM_M_CALM, 70 },
  { "思考", "Think", DM_M_THINK, 55 },
  { "惊讶", "Surprise", DM_M_SURPRISE, 60 },
  { "难过", "Sad", DM_M_SAD, 25 },
  { "生气", "Angry", DM_M_ANGRY, 15 },
  { "疲惫", "Tired", DM_M_TIRED, 35 },
  { "焦虑", "Anxious", DM_M_ANXIOUS, 20 },
  { "孤独", "Lonely", DM_M_LONELY, 30 },
  { "压力", "Stress", DM_M_STRESS, 20 },
};

static const char *s_quick_zh[DM_QUICK_MAX] = {
  "今天心情不错", "感觉一般般", "有点不开心", "很累想休息",
  "充满活力", "心平气和", "坐立不安", "若有所思",
};
static const char *s_quick_en[DM_QUICK_MAX] = {
  "Pretty good", "Just okay", "A bit down", "Tired",
  "Energetic", "Calm", "Restless", "Pensive",
};
static const int s_quick_score[DM_QUICK_MAX] = {
  78, 50, 32, 38, 88, 75, 35, 65,
};

static dm_mood_rec_t s_recs[DM_MOOD_REC_MAX];
static int s_rec_n;

static lv_obj_t *s_page;
static lv_obj_t *s_sc;
static lv_obj_t *s_score_lbl;
static lv_obj_t *s_tag_lbl;
static lv_obj_t *s_msg_lbl;
static lv_obj_t *s_focus_lbl;
static lv_obj_t *s_list_box;
static lv_obj_t *s_fab;

static lv_obj_t *s_log_page;
static lv_obj_t *s_mood_btns[16];
static lv_obj_t *s_quick_btns[DM_QUICK_MAX];
static lv_obj_t *s_seg_cur;
static lv_obj_t *s_seg_day;
static uint16_t s_pick_mask;
static uint8_t s_pick_quick;
static uint8_t s_pick_type;

/* ---------- storage ---------- */

typedef struct
{
  uint32_t magic;
  uint32_t count;
  int32_t focus_done_min;
  dm_mood_rec_t recs[DM_MOOD_REC_MAX];
} dm_mood_file_t;

static void store_save(void)
{
  dm_mood_file_t f;
  int fd;

  memset(&f, 0, sizeof(f));
  f.magic = DM_MOOD_MAGIC;
  f.count = (uint32_t)s_rec_n;
  f.focus_done_min = g_dm.focus_done_min;
  if (s_rec_n > 0)
    {
      memcpy(f.recs, s_recs, sizeof(dm_mood_rec_t) * (size_t)s_rec_n);
    }

  fd = open(DM_MOOD_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      return;
    }
  (void)write(fd, &f, sizeof(f));
  close(fd);
}

static void store_load(void)
{
  dm_mood_file_t f;
  int fd;
  ssize_t n;

  fd = open(DM_MOOD_PATH, O_RDONLY);
  if (fd < 0)
    {
      return;
    }
  n = read(fd, &f, sizeof(f));
  close(fd);
  if (n != (ssize_t)sizeof(f) || f.magic != DM_MOOD_MAGIC)
    {
      return;
    }
  if (f.count > DM_MOOD_REC_MAX)
    {
      f.count = DM_MOOD_REC_MAX;
    }
  s_rec_n = (int)f.count;
  if (s_rec_n > 0)
    {
      memcpy(s_recs, f.recs, sizeof(dm_mood_rec_t) * (size_t)s_rec_n);
    }
  g_dm.focus_done_min = f.focus_done_min;
}

/* ---------- score ---------- */

static int mood_mask_score(uint16_t mask)
{
  int sum = 0;
  int n = 0;
  int i;

  if (mask == 0)
    {
      return -1;
    }
  for (i = 0; i < (int)(sizeof(s_moods) / sizeof(s_moods[0])); i++)
    {
      if (mask & s_moods[i].bit)
        {
          sum += s_moods[i].score;
          n++;
        }
    }
  return n ? (sum / n) : -1;
}

static int focus_score_from_min(int32_t min)
{
  if (min <= 0)
    {
      return 40;
    }
  if (min >= 60)
    {
      return 95;
    }
  return (int)(40 + min * 55 / 60);
}

int dm_health_score(void)
{
  int mood_sum = 0;
  int mood_n = 0;
  int i;
  int ms;
  int fs;
  int total;

  for (i = 0; i < s_rec_n; i++)
    {
      int one = mood_mask_score(s_recs[i].mood_mask);
      if (one < 0 && s_recs[i].quick > 0 && s_recs[i].quick <= DM_QUICK_MAX)
        {
          one = s_quick_score[s_recs[i].quick - 1];
        }
      else if (one >= 0 && s_recs[i].quick > 0 &&
               s_recs[i].quick <= DM_QUICK_MAX)
        {
          one = (one + s_quick_score[s_recs[i].quick - 1]) / 2;
        }
      if (one >= 0)
        {
          mood_sum += one;
          mood_n++;
        }
    }

  ms = mood_n ? (mood_sum / mood_n) : -1;
  fs = focus_score_from_min(g_dm.focus_done_min);
  total = (ms < 0) ? fs : ((ms * 6 + fs * 4) / 10);
  if (total < 0)
    {
      total = 0;
    }
  if (total > 100)
    {
      total = 100;
    }
  return total;
}

static uint32_t score_color(int v)
{
  if (v >= 60)
    {
      return C_OK;
    }
  if (v >= 40)
    {
      return C_STAR;
    }
  return C_HEART;
}

static const char *score_tag_zh(int v)
{
  if (v >= 80)
    {
      return "状态很好";
    }
  if (v >= 60)
    {
      return "状态良好";
    }
  if (v >= 40)
    {
      return "有些波动";
    }
  return "需要关照";
}

static const char *score_tag_en(int v)
{
  if (v >= 80)
    {
      return "Great";
    }
  if (v >= 60)
    {
      return "Good";
    }
  if (v >= 40)
    {
      return "Wavy";
    }
  return "Take care";
}

static const char *score_msg_zh(int v)
{
  if (v >= 80)
    {
      return "正向记录+专注都不错";
    }
  if (v >= 60)
    {
      return "综合状态良好，累了记得休息";
    }
  if (v >= 40)
    {
      return "情绪或专注有起伏，走动一下吧";
    }
  return "负向偏多，我陪你慢一点";
}

static const char *score_msg_en(int v)
{
  if (v >= 80)
    {
      return "Mood and focus look strong";
    }
  if (v >= 60)
    {
      return "Doing well — rest if tired";
    }
  if (v >= 40)
    {
      return "Some ups and downs.";
    }
  return "Rough patch. Take it easy.";
}

/* ---------- helpers ---------- */

static lv_obj_t *mk_page(lv_obj_t *parent, dm_page_t id)
{
  lv_obj_t *page = lv_obj_create(parent);
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

static lv_obj_t *mk_card(lv_obj_t *parent, int h)
{
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_set_width(c, 292);
  lv_obj_set_height(c, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 10, LV_PART_MAIN);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

static void now_hm(uint8_t *h, uint8_t *m)
{
  time_t t = time(NULL);
  struct tm tmv;
  localtime_r(&t, &tmv);
  *h = (uint8_t)tmv.tm_hour;
  *m = (uint8_t)tmv.tm_min;
}

void dm_health_add_focus_min(int32_t min)
{
  if (min > 0)
    {
      g_dm.focus_done_min += min;
      if (g_dm.focus_done_min > 600)
        {
          g_dm.focus_done_min = 600;
        }
      store_save();
    }
}

static void rebuild_list(void)
{
  int i;
  char line[64];

  if (!s_list_box)
    {
      return;
    }
  lv_obj_clean(s_list_box);

  if (s_rec_n == 0)
    {
      lv_obj_t *e = dm_lbl(s_list_box, "还没有记录，点「记录心情」添加",
                           "No records yet", g_dm_font_s, C_MUTED);
      lv_label_set_long_mode(e, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(e, 260);
      return;
    }

  for (i = 0; i < s_rec_n; i++)
    {
      lv_obj_t *row = lv_obj_create(s_list_box);
      lv_obj_set_width(row, 270);
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

      char tags[48];
      int pos = 0;
      int ti;

      tags[0] = 0;
      pos = snprintf(tags, sizeof(tags), "%s",
                     s_recs[i].rec_type ? dm_t("今日整体", "Daily")
                                        : dm_t("当前心情", "Now"));
      for (ti = 0; ti < (int)(sizeof(s_moods) / sizeof(s_moods[0])); ti++)
        {
          if (s_recs[i].mood_mask & s_moods[ti].bit)
            {
              pos += snprintf(tags + pos, sizeof(tags) - pos, " · %s",
                              dm_t(s_moods[ti].zh, s_moods[ti].en));
              if (pos >= (int)sizeof(tags) - 6)
                {
                  break;
                }
            }
        }

      lv_obj_t *head = dm_lbl(row, tags, tags, g_dm_font_s, C_INK);
      lv_label_set_long_mode(head, LV_LABEL_LONG_DOT);
      lv_obj_set_width(head, 200);

      if (s_recs[i].quick > 0 && s_recs[i].quick <= DM_QUICK_MAX)
        {
          lv_obj_t *q = dm_lbl(row, s_quick_zh[s_recs[i].quick - 1],
                               s_quick_en[s_recs[i].quick - 1],
                               g_dm_font_s, C_DIM);
          lv_obj_set_pos(q, 0, 16);
        }

      lv_snprintf(line, sizeof(line), "%02u:%02u", s_recs[i].hour,
                  s_recs[i].min);
      lv_obj_t *time_l = lv_label_create(row);
      lv_label_set_text(time_l, line);
      dm_style(time_l, g_dm_font_s, C_MUTED);
      lv_obj_align(time_l, LV_ALIGN_TOP_RIGHT, 0, 0);

      lv_obj_set_height(row, s_recs[i].quick ? 34 : 20);
    }
}

void dm_health_refresh(void)
{
  int v = dm_health_score();
  char b[48];

  if (s_score_lbl)
    {
      lv_snprintf(b, sizeof(b), "%d", v);
      lv_label_set_text(s_score_lbl, b);
      lv_obj_set_style_text_color(s_score_lbl, lv_color_hex(score_color(v)),
                                  LV_PART_MAIN);
    }
  if (s_tag_lbl)
    {
      lv_label_set_text(s_tag_lbl, dm_t(score_tag_zh(v), score_tag_en(v)));
      lv_obj_set_style_text_color(s_tag_lbl, lv_color_hex(score_color(v)),
                                  LV_PART_MAIN);
    }
  if (s_msg_lbl)
    {
      lv_label_set_text(s_msg_lbl, dm_t(score_msg_zh(v), score_msg_en(v)));
    }
  if (s_focus_lbl)
    {
      lv_snprintf(b, sizeof(b), "%s %ld min · %s %d",
                  dm_t("专注", "Focus"), (long)g_dm.focus_done_min,
                  dm_t("心情", "Mood"), s_rec_n);
      lv_label_set_text(s_focus_lbl, b);
    }
  rebuild_list();
}

/* ---------- mood log page ---------- */

static lv_obj_t *first_label(lv_obj_t *btn)
{
  uint32_t n;
  uint32_t i;

  if (!btn || !lv_obj_is_valid(btn))
    {
      return NULL;
    }
  n = lv_obj_get_child_count(btn);
  for (i = 0; i < n; i++)
    {
      lv_obj_t *c = lv_obj_get_child(btn, i);
      if (c && lv_obj_check_type(c, &lv_label_class))
        {
          return c;
        }
    }
  return NULL;
}

static void style_chip(lv_obj_t *btn, uint32_t bg, uint32_t fg)
{
  lv_obj_t *lab;

  if (!btn || !lv_obj_is_valid(btn))
    {
      return;
    }
  lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lab = first_label(btn);
  if (lab)
    {
      lv_obj_set_style_text_color(lab, lv_color_hex(fg), LV_PART_MAIN);
    }
}

static void paint_log_picks(void)
{
  int i;

  for (i = 0; i < 16; i++)
    {
      if (!s_mood_btns[i])
        {
          continue;
        }
      if (s_pick_mask & s_moods[i].bit)
        {
          style_chip(s_mood_btns[i],
                     (s_moods[i].bit & M_NEG) ? C_HEART : C_FACE, C_EYE);
        }
      else
        {
          style_chip(s_mood_btns[i], C_BTN_HI, C_DIM);
        }
    }

  for (i = 0; i < DM_QUICK_MAX; i++)
    {
      if (!s_quick_btns[i])
        {
          continue;
        }
      if (s_pick_quick == i + 1)
        {
          style_chip(s_quick_btns[i], C_ACCENT, C_EYE);
        }
      else
        {
          style_chip(s_quick_btns[i], C_BTN, C_MUTED);
        }
    }

  if (s_seg_cur && s_seg_day)
    {
      style_chip(s_seg_cur, s_pick_type == 0 ? C_FACE : C_BTN,
                 s_pick_type == 0 ? C_EYE : C_MUTED);
      style_chip(s_seg_day, s_pick_type ? C_FACE : C_BTN,
                 s_pick_type ? C_EYE : C_MUTED);
    }
}

static void open_log_page(lv_event_t *e)
{
  (void)e;
  s_pick_mask = 0;
  s_pick_quick = 0;
  s_pick_type = 0;
  paint_log_picks();
  dm_show(PAGE_MOOD_LOG);
}

static void back_health(lv_event_t *e)
{
  (void)e;
  dm_health_refresh();
  dm_show(PAGE_HEALTH);
}

static void mood_btn_cb(lv_event_t *e)
{
  int idx = (int)(uintptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= 16)
    {
      return;
    }
  s_pick_mask ^= s_moods[idx].bit;
  paint_log_picks();
}

static void quick_btn_cb(lv_event_t *e)
{
  int idx = (int)(uintptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= DM_QUICK_MAX)
    {
      return;
    }
  if (s_pick_quick == idx + 1)
    {
      s_pick_quick = 0;
    }
  else
    {
      s_pick_quick = (uint8_t)(idx + 1);
    }
  paint_log_picks();
}

static void seg_cb(lv_event_t *e)
{
  s_pick_type = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  paint_log_picks();
}

static void save_cb(lv_event_t *e)
{
  dm_mood_rec_t rec;
  (void)e;

  if (s_pick_mask == 0 && s_pick_quick == 0)
    {
      /* stay on page; could flash hint */
      return;
    }

  memset(&rec, 0, sizeof(rec));
  rec.mood_mask = s_pick_mask;
  rec.quick = s_pick_quick;
  rec.rec_type = s_pick_type;
  now_hm(&rec.hour, &rec.min);

  if (s_rec_n < DM_MOOD_REC_MAX)
    {
      memmove(&s_recs[1], &s_recs[0], sizeof(dm_mood_rec_t) * (size_t)s_rec_n);
      s_recs[0] = rec;
      s_rec_n++;
    }
  else
    {
      memmove(&s_recs[1], &s_recs[0],
              sizeof(dm_mood_rec_t) * (DM_MOOD_REC_MAX - 1));
      s_recs[0] = rec;
    }

  store_save();
  dm_health_refresh();
  dm_show(PAGE_HEALTH);
}

/* ---------- build ---------- */

static lv_obj_t *chip_btn(lv_obj_t *p, const char *zh, const char *en, int w,
                          int h, lv_event_cb_t cb, void *ud)
{
  return dm_btn(p, zh, en, w, h, C_BTN_HI, C_DIM, cb, ud);
}

void dm_create_health(void)
{
  lv_obj_t *title;
  lv_obj_t *score_card;
  lv_obj_t *num;
  lv_obj_t *unit;
  lv_obj_t *focus_card;
  lv_obj_t *list_card;
  lv_obj_t *list_t;
  int i;
  int x;
  int y;

  store_load();

  s_page = mk_page(g_dm_root, PAGE_HEALTH);

  title = dm_lbl(s_page, "健康", "Health", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 12, 8);

  s_sc = lv_obj_create(s_page);
  lv_obj_set_size(s_sc, DM_SCR_W, 160);
  lv_obj_set_pos(s_sc, 0, 28);
  lv_obj_set_style_bg_opa(s_sc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_sc, 6, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_sc, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(s_sc, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_sc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_sc, 8, LV_PART_MAIN);

  score_card = mk_card(s_sc, 72);
  {
    lv_obj_t *cap = dm_lbl(score_card, "情绪综合分", "Mood score",
                           g_dm_font_s, C_MUTED);
    lv_obj_set_pos(cap, 0, 0);
    num = lv_label_create(score_card);
    lv_label_set_text(num, "60");
    dm_style(num, g_dm_font_l, C_OK);
    lv_obj_set_pos(num, 0, 22);
    s_score_lbl = num;
    unit = dm_lbl(score_card, "/100", "/100", g_dm_font_s, C_MUTED);
    lv_obj_set_pos(unit, 40, 30);
    s_tag_lbl = dm_lbl(score_card, "状态良好", "Good", g_dm_font_s, C_OK);
    lv_obj_set_pos(s_tag_lbl, 90, 22);
    s_msg_lbl = dm_lbl(score_card, "综合心情与专注", "Mood + focus",
                       g_dm_font_s, C_DIM);
    lv_label_set_long_mode(s_msg_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_msg_lbl, 180);
    lv_obj_set_pos(s_msg_lbl, 90, 40);
  }

  focus_card = mk_card(s_sc, 40);
  s_focus_lbl = dm_lbl(focus_card, "专注 0 min · 心情 0",
                       "Focus 0 · Mood 0", g_dm_font_s, C_INK);
  lv_obj_center(s_focus_lbl);

  list_card = mk_card(s_sc, 120);
  list_t = dm_lbl(list_card, "最近心情", "Recent moods", g_dm_font_s,
                  C_MUTED);
  lv_obj_set_pos(list_t, 0, 0);
  s_list_box = lv_obj_create(list_card);
  lv_obj_set_size(s_list_box, 260, 90);
  lv_obj_set_pos(s_list_box, 0, 18);
  lv_obj_set_style_bg_opa(s_list_box, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_list_box, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_list_box, 0, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_list_box, LV_DIR_VER);
  lv_obj_set_flex_flow(s_list_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s_list_box, 2, LV_PART_MAIN);

  {
    lv_obj_t *tipc = mk_card(s_sc, 56);
    lv_obj_t *tt = dm_lbl(tipc, "说明", "Note", g_dm_font_s, C_MUTED);
    lv_obj_t *tb = dm_lbl(tipc,
                          "评分 = 心情 + 今日专注\n保存后会写入本地",
                          "Score = mood + focus\nSaved locally",
                          g_dm_font_s, C_DIM);
    lv_obj_set_pos(tt, 0, 0);
    lv_label_set_long_mode(tb, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(tb, 250);
    lv_obj_set_pos(tb, 0, 16);
  }

  s_fab = dm_btn(s_page, "记录心情", "Log mood", 120, 30, C_FACE, C_EYE,
                 open_log_page, NULL);
  lv_obj_align(s_fab, LV_ALIGN_TOP_MID, 0, 160);

  /* ---- full-page mood log ---- */
  s_log_page = mk_page(g_dm_root, PAGE_MOOD_LOG);

  {
    lv_obj_t *ht = dm_lbl(s_log_page, "记录心情", "Log mood", g_dm_font_m,
                          C_INK);
    lv_obj_set_pos(ht, 12, 8);

    lv_obj_t *back = dm_btn(s_log_page, "返回", "Back", 56, 26, C_BTN,
                            C_MUTED, back_health, NULL);
    lv_obj_set_pos(back, 252, 6);

    s_seg_cur = chip_btn(s_log_page, "当前", "Now", 90, 26, seg_cb,
                         (void *)(uintptr_t)0);
    lv_obj_set_pos(s_seg_cur, 12, 40);
    s_seg_day = chip_btn(s_log_page, "今日整体", "Daily", 100, 26, seg_cb,
                         (void *)(uintptr_t)1);
    lv_obj_set_pos(s_seg_day, 110, 40);

    x = 12;
    y = 72;
    for (i = 0; i < 16; i++)
      {
        int w = 72;
        s_mood_btns[i] = chip_btn(s_log_page, s_moods[i].zh, s_moods[i].en,
                                  w, 24, mood_btn_cb, (void *)(uintptr_t)i);
        lv_obj_set_pos(s_mood_btns[i], x, y);
        x += w + 4;
        if (x > 240)
          {
            x = 12;
            y += 26;
          }
      }

    x = 12;
    y = 180;
    for (i = 0; i < DM_QUICK_MAX; i++)
      {
        int w = 72;
        s_quick_btns[i] = chip_btn(s_log_page, s_quick_zh[i], s_quick_en[i],
                                   w, 22, quick_btn_cb,
                                   (void *)(uintptr_t)i);
        lv_obj_set_pos(s_quick_btns[i], x, y);
        x += w + 4;
        if (x > 240)
          {
            x = 12;
            y += 24;
          }
      }
  }

  {
    lv_obj_t *save = dm_btn(s_log_page, "保存", "Save", 70, 26, C_FACE,
                            C_EYE, save_cb, NULL);
    lv_obj_set_pos(save, 174, 6);
  }

  paint_log_picks();
  dm_health_refresh();
}

#endif /* CONFIG_DESKMATE_APP */
