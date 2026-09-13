/****************************************************************************
 * dm_ui_health.c — scrollable health page + manual mood log
 *
 * Score = blend of manual mood records and completed focus minutes.
 * No free-text input (quick phrases only).
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef CONFIG_DESKMATE_APP

#define M_POS (DM_M_JOY | DM_M_LOVE | DM_M_EXCITE | DM_M_CONFIDENT | \
               DM_M_EXPECT | DM_M_SATISFY | DM_M_TOUCHED | DM_M_CALM)
#define M_NEG (DM_M_SAD | DM_M_ANGRY | DM_M_TIRED | DM_M_ANXIOUS | \
               DM_M_LONELY | DM_M_STRESS)
#define M_NEU (DM_M_THINK | DM_M_SURPRISE)

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
static lv_obj_t *s_overlay;
static lv_obj_t *s_sheet;
static lv_obj_t *s_mood_btns[16];
static lv_obj_t *s_quick_btns[DM_QUICK_MAX];
static lv_obj_t *s_seg_cur;
static lv_obj_t *s_seg_day;
static uint16_t s_pick_mask;
static uint8_t s_pick_quick;
static uint8_t s_pick_type;

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
      if (one < 0 && s_recs[i].quick > 0 &&
          s_recs[i].quick <= DM_QUICK_MAX)
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

  if (ms < 0)
    {
      total = fs;
    }
  else
    {
      total = (ms * 6 + fs * 4) / 10;
    }
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
  if (v >= 80)
    {
      return C_OK;
    }
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
      return "正向记录+专注都不错，保持节奏";
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
      return "Some ups and downs. Stretch a bit";
    }
  return "Rough patch. I'll go easy with you";
}

/* ---------- helpers ---------- */

static lv_obj_t *mk_card(lv_obj_t *parent, int h)
{
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
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
    }
}

/* ---------- UI refresh ---------- */

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
      lv_obj_t *e = dm_lbl(s_list_box, "还没有记录，点下面按钮添加",
                           "No records yet. Tap the button",
                           g_dm_font_s, C_MUTED);
      lv_label_set_long_mode(e, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(e, 260);
      return;
    }

  for (i = 0; i < s_rec_n; i++)
    {
      lv_obj_t *row;
      lv_obj_t *head;
      lv_obj_t *time_l;
      char tags[40];
      int ti;
      int pos = 0;

      row = lv_obj_create(s_list_box);
      lv_obj_set_width(row, 270);
      lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_ver(row, 4, LV_PART_MAIN);
      lv_obj_set_style_pad_hor(row, 0, LV_PART_MAIN);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

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
              if (pos >= (int)sizeof(tags) - 8)
                {
                  break;
                }
            }
        }
      head = dm_lbl(row, tags, tags, g_dm_font_s, C_INK);
      lv_label_set_long_mode(head, LV_LABEL_LONG_DOT);
      lv_obj_set_width(head, 200);

      if (s_recs[i].quick > 0 && s_recs[i].quick <= DM_QUICK_MAX)
        {
          lv_obj_t *q = dm_lbl(row,
                               s_quick_zh[s_recs[i].quick - 1],
                               s_quick_en[s_recs[i].quick - 1],
                               g_dm_font_s, C_DIM);
          lv_obj_set_pos(q, 0, 16);
        }

      lv_snprintf(line, sizeof(line), "%02d:%02d", s_recs[i].hour,
                  s_recs[i].min);
      time_l = lv_label_create(row);
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

/* ---------- record sheet ---------- */

static void paint_sheet_picks(void)
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
          uint32_t bg = C_FACE;
          if (s_moods[i].bit & M_NEG)
            {
              bg = C_HEART;
            }
          lv_obj_set_style_bg_color(s_mood_btns[i], lv_color_hex(bg),
                                    LV_PART_MAIN);
          lv_obj_set_style_text_color(lv_obj_get_child(s_mood_btns[i], 0),
                                      lv_color_hex(C_EYE), LV_PART_MAIN);
        }
      else
        {
          lv_obj_set_style_bg_color(s_mood_btns[i], lv_color_hex(C_BTN_HI),
                                    LV_PART_MAIN);
          lv_obj_set_style_text_color(lv_obj_get_child(s_mood_btns[i], 0),
                                      lv_color_hex(C_DIM), LV_PART_MAIN);
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
          lv_obj_set_style_bg_color(s_quick_btns[i], lv_color_hex(C_ACCENT),
                                    LV_PART_MAIN);
          lv_obj_set_style_text_color(lv_obj_get_child(s_quick_btns[i], 0),
                                      lv_color_hex(C_EYE), LV_PART_MAIN);
        }
      else
        {
          lv_obj_set_style_bg_color(s_quick_btns[i], lv_color_hex(C_BTN),
                                    LV_PART_MAIN);
          lv_obj_set_style_text_color(lv_obj_get_child(s_quick_btns[i], 0),
                                      lv_color_hex(C_MUTED), LV_PART_MAIN);
        }
    }

  if (s_seg_cur && s_seg_day)
    {
      if (s_pick_type == 0)
        {
          lv_obj_set_style_bg_color(s_seg_cur, lv_color_hex(C_FACE),
                                    LV_PART_MAIN);
          lv_obj_set_style_bg_color(s_seg_day, lv_color_hex(C_BTN),
                                    LV_PART_MAIN);
        }
      else
        {
          lv_obj_set_style_bg_color(s_seg_cur, lv_color_hex(C_BTN),
                                    LV_PART_MAIN);
          lv_obj_set_style_bg_color(s_seg_day, lv_color_hex(C_FACE),
                                    LV_PART_MAIN);
        }
    }
}

static void open_sheet(lv_event_t *e)
{
  (void)e;
  s_pick_mask = 0;
  s_pick_quick = 0;
  s_pick_type = 0;
  paint_sheet_picks();
  if (s_overlay)
    {
      lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void close_sheet(lv_event_t *e)
{
  (void)e;
  if (s_overlay)
    {
      lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void mood_btn_cb(lv_event_t *e)
{
  int idx = (int)(uintptr_t)lv_event_get_user_data(e);
  if (idx < 0 || idx >= 16)
    {
      return;
    }
  s_pick_mask ^= s_moods[idx].bit;
  paint_sheet_picks();
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
  paint_sheet_picks();
}

static void seg_cb(lv_event_t *e)
{
  s_pick_type = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
  paint_sheet_picks();
}

static void save_cb(lv_event_t *e)
{
  dm_mood_rec_t rec;
  (void)e;

  if (s_pick_mask == 0 && s_pick_quick == 0)
    {
      dm_say("先选一个心情", "Pick a mood first");
      return;
    }

  memset(&rec, 0, sizeof(rec));
  rec.mood_mask = s_pick_mask;
  rec.quick = s_pick_quick;
  rec.rec_type = s_pick_type;
  now_hm(&rec.hour, &rec.min);

  if (s_rec_n < DM_MOOD_REC_MAX)
    {
      memmove(&s_recs[1], &s_recs[0], sizeof(dm_mood_rec_t) * s_rec_n);
      s_recs[0] = rec;
      s_rec_n++;
    }
  else
    {
      memmove(&s_recs[1], &s_recs[0],
              sizeof(dm_mood_rec_t) * (DM_MOOD_REC_MAX - 1));
      s_recs[0] = rec;
    }

  close_sheet(NULL);
  dm_health_refresh();
  dm_say("已记下，我会看着你的", "Logged. I'll keep an eye on you");
}

/* ---------- build ---------- */

static lv_obj_t *sheet_btn(lv_obj_t *p, const char *zh, const char *en, int w,
                           int h, lv_event_cb_t cb, void *ud)
{
  lv_obj_t *b = dm_btn(p, zh, en, w, h, C_BTN_HI, C_DIM, cb, ud);
  return b;
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

  s_page = lv_obj_create(g_dm_root);
  lv_obj_set_size(s_page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(s_page, 0, 0);
  lv_obj_set_style_bg_color(s_page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_HEALTH] = s_page;

  title = dm_lbl(s_page, "健康", "Health", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 12, 8);

  /* scroll area above dock (dock ~46) and fab */
  s_sc = lv_obj_create(s_page);
  lv_obj_set_size(s_sc, DM_SCR_W, 160);
  lv_obj_set_pos(s_sc, 0, 28);
  lv_obj_set_style_bg_opa(s_sc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_sc, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(s_sc, 4, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_sc, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(s_sc, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_sc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_sc, 8, LV_PART_MAIN);

  /* score */
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
    s_msg_lbl = dm_lbl(score_card, "综合心情与专注", "Mood + focus blend",
                       g_dm_font_s, C_DIM);
    lv_label_set_long_mode(s_msg_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_msg_lbl, 180);
    lv_obj_set_pos(s_msg_lbl, 90, 40);
  }

  /* focus + count */
  focus_card = mk_card(s_sc, 40);
  s_focus_lbl = dm_lbl(focus_card, "专注 0 min · 心情 0",
                       "Focus 0 min · Mood 0", g_dm_font_s, C_INK);
  lv_obj_center(s_focus_lbl);

  /* list */
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

  /* tip card */
  {
    lv_obj_t *tipc = mk_card(s_sc, 64);
    lv_obj_t *tt = dm_lbl(tipc, "说明", "Note", g_dm_font_s, C_MUTED);
    lv_obj_t *tb = dm_lbl(tipc,
                          "评分 = 心情记录 + 今日专注时长\n仅供参考，不能替代专业诊断",
                          "Score = moods + focus today\nFor reference only",
                          g_dm_font_s, C_DIM);
    lv_obj_set_pos(tt, 0, 0);
    lv_label_set_long_mode(tb, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(tb, 250);
    lv_obj_set_pos(tb, 0, 16);
  }

  /* FAB */
  s_fab = dm_btn(s_page, "＋ 记录心情", "Log mood", 120, 30, C_FACE, C_EYE,
                 open_sheet, NULL);
  lv_obj_align(s_fab, LV_ALIGN_TOP_MID, 0, 160);

  /* overlay + bottom sheet (scrollable body) */
  s_overlay = lv_obj_create(s_page);
  lv_obj_remove_style_all(s_overlay);
  lv_obj_set_size(s_overlay, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(s_overlay, 0, 0);
  lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_70, LV_PART_MAIN);
  lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);

  s_sheet = lv_obj_create(s_overlay);
  lv_obj_remove_style_all(s_sheet);
  lv_obj_set_size(s_sheet, DM_SCR_W, 220);
  lv_obj_set_pos(s_sheet, 0, 20);
  lv_obj_set_style_bg_color(s_sheet, lv_color_hex(0x121212), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_sheet, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(s_sheet, 14, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_sheet, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_sheet, LV_OBJ_FLAG_SCROLLABLE);

  {
    lv_obj_t *ht = dm_lbl(s_sheet, "记录心情", "Log mood", g_dm_font_m,
                          C_INK);
    lv_obj_set_pos(ht, 12, 8);

    lv_obj_t *hint = dm_lbl(s_sheet, "上下滑动选择", "Scroll to pick",
                            g_dm_font_s, C_MUTED);
    lv_obj_set_pos(hint, 200, 12);

    lv_obj_t *body = lv_obj_create(s_sheet);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, DM_SCR_W, 150);
    lv_obj_set_pos(body, 0, 32);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_row(body, 6, LV_PART_MAIN);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    {
      lv_obj_t *seg = lv_obj_create(body);
      lv_obj_remove_style_all(seg);
      lv_obj_set_size(seg, 296, 28);
      lv_obj_set_style_bg_opa(seg, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
      s_seg_cur = sheet_btn(seg, "当前", "Now", 90, 26, seg_cb,
                            (void *)(uintptr_t)0);
      lv_obj_set_pos(s_seg_cur, 0, 0);
      s_seg_day = sheet_btn(seg, "今日整体", "Daily", 110, 26, seg_cb,
                            (void *)(uintptr_t)1);
      lv_obj_set_pos(s_seg_day, 98, 0);
    }

    {
      lv_obj_t *lab = dm_lbl(body, "心情（可多选）", "Moods (multi)",
                             g_dm_font_s, C_MUTED);
      lv_obj_set_size(lab, 296, 14);
    }

    {
      lv_obj_t *grid = lv_obj_create(body);
      lv_obj_remove_style_all(grid);
      lv_obj_set_size(grid, 296, 120);
      lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

      x = 0;
      y = 0;
      for (i = 0; i < 16; i++)
        {
          int w = 48;
          s_mood_btns[i] = sheet_btn(grid, s_moods[i].zh, s_moods[i].en, w,
                                     24, mood_btn_cb, (void *)(uintptr_t)i);
          lv_obj_set_pos(s_mood_btns[i], x, y);
          x += w + 4;
          if (x > 250)
            {
              x = 0;
              y += 28;
            }
        }
    }

    {
      lv_obj_t *lab = dm_lbl(body, "快捷描述", "Quick note", g_dm_font_s,
                             C_MUTED);
      lv_obj_set_size(lab, 296, 14);
    }

    {
      lv_obj_t *qgrid = lv_obj_create(body);
      lv_obj_remove_style_all(qgrid);
      lv_obj_set_size(qgrid, 296, 80);
      lv_obj_set_style_bg_opa(qgrid, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_clear_flag(qgrid, LV_OBJ_FLAG_SCROLLABLE);

      x = 0;
      y = 0;
      for (i = 0; i < DM_QUICK_MAX; i++)
        {
          int w = 72;
          s_quick_btns[i] = sheet_btn(qgrid, s_quick_zh[i], s_quick_en[i],
                                      w, 24, quick_btn_cb,
                                      (void *)(uintptr_t)i);
          lv_obj_set_pos(s_quick_btns[i], x, y);
          x += w + 4;
          if (x > 220)
            {
              x = 0;
              y += 28;
            }
        }
    }
  }

  {
    lv_obj_t *foot = lv_obj_create(s_sheet);
    lv_obj_remove_style_all(foot);
    lv_obj_set_size(foot, DM_SCR_W, 36);
    lv_obj_set_pos(foot, 0, 184);
    lv_obj_set_style_bg_color(foot, lv_color_hex(0x0a0a0a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(foot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(foot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cancel = dm_btn(foot, "取消", "Cancel", 110, 28, C_BTN,
                              C_MUTED, close_sheet, NULL);
    lv_obj_t *save = dm_btn(foot, "保存", "Save", 110, 28, C_FACE, C_EYE,
                            save_cb, NULL);
    lv_obj_set_pos(cancel, 36, 4);
    lv_obj_set_pos(save, 166, 4);
  }

  if (s_rec_n == 0)
    {
      dm_mood_rec_t rec;
      memset(&rec, 0, sizeof(rec));
      rec.mood_mask = DM_M_CALM;
      rec.rec_type = 0;
      now_hm(&rec.hour, &rec.min);
      s_recs[0] = rec;
      s_rec_n = 1;
      g_dm.focus_done_min = 25;
    }

  paint_sheet_picks();
  dm_health_refresh();
}

#endif /* CONFIG_DESKMATE_APP */
