/****************************************************************************
 * dm_ui_tools.c — Calculator / Sensors / Guess / Convert
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <sensor/temp.h>
#include <sensor/humi.h>
#include <sensor/light.h>
#include <uORB/uORB.h>

#ifdef CONFIG_DESKMATE_APP

/* ---------- shared shell ---------- */

static void tools_back(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_FEATURES);
}

static lv_obj_t *mk_tools_page(dm_page_t id, const char *zh, const char *en)
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

  back = dm_btn(page, "←", "<", 36, 24, C_BTN, C_MUTED, tools_back, NULL);
  lv_obj_set_pos(back, 8, 6);

  title = dm_lbl(page, zh, en, g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);

  g_dm_pages[id] = page;
  return page;
}

/* ---------- calculator ---------- */

#define CALC_EXPR_MAX 48
#define CALC_CUR_MAX 16

static lv_obj_t *s_calc_expr_l;
static lv_obj_t *s_calc_res_l;
static char s_calc_expr[CALC_EXPR_MAX];
static char s_calc_cur[CALC_CUR_MAX];
static double s_calc_acc;
static char s_calc_op; /* 0, or + - * / */
static bool s_calc_typing;
static bool s_calc_just_eq;

static void calc_show(void)
{
  if (s_calc_expr_l)
    {
      lv_label_set_text(s_calc_expr_l, s_calc_expr[0] ? s_calc_expr : " ");
    }
  if (s_calc_res_l)
    {
      lv_label_set_text(s_calc_res_l, s_calc_cur);
    }
}

static void calc_fmt(double v, char *out, size_t n)
{
  if (!isfinite(v))
    {
      snprintf(out, n, "ERR");
      return;
    }
  if (fabs(v) >= 1e10 || (fabs(v) < 1e-6 && v != 0.0))
    {
      snprintf(out, n, "%.6g", v);
      return;
    }
  snprintf(out, n, "%.10g", v);
}

static double calc_do(double a, char op, double b)
{
  switch (op)
    {
    case '+':
      return a + b;
    case '-':
      return a - b;
    case '*':
      return a * b;
    case '/':
      return (b == 0.0) ? NAN : a / b;
    default:
      return b;
    }
}

static void calc_expr_acc_op(char op)
{
  char nb[CALC_CUR_MAX];
  calc_fmt(s_calc_acc, nb, sizeof(nb));
  if (op)
    {
      snprintf(s_calc_expr, sizeof(s_calc_expr), "%s %c", nb, op);
    }
  else
    {
      snprintf(s_calc_expr, sizeof(s_calc_expr), "%s", nb);
    }
}

static void calc_reset(void)
{
  s_calc_expr[0] = 0;
  snprintf(s_calc_cur, sizeof(s_calc_cur), "0");
  s_calc_acc = 0.0;
  s_calc_op = 0;
  s_calc_typing = false;
  s_calc_just_eq = false;
  calc_show();
}

static void calc_digit(char c)
{
  if (s_calc_just_eq)
    {
      s_calc_expr[0] = 0;
      s_calc_acc = 0.0;
      s_calc_op = 0;
      s_calc_just_eq = false;
      s_calc_typing = false;
      snprintf(s_calc_cur, sizeof(s_calc_cur), "0");
    }

  if (!s_calc_typing)
    {
      if (c == '.')
        {
          snprintf(s_calc_cur, sizeof(s_calc_cur), "0.");
        }
      else
        {
          s_calc_cur[0] = c;
          s_calc_cur[1] = 0;
        }
      s_calc_typing = true;
    }
  else
    {
      size_t n = strlen(s_calc_cur);
      if (c == '.' && strchr(s_calc_cur, '.'))
        {
          return;
        }
      if (n + 1 < sizeof(s_calc_cur))
        {
          s_calc_cur[n] = c;
          s_calc_cur[n + 1] = 0;
        }
    }
  calc_show();
}

static void calc_op(char op)
{
  if (s_calc_just_eq)
    {
      s_calc_acc = atof(s_calc_cur);
      s_calc_just_eq = false;
      s_calc_typing = false;
      s_calc_op = op;
      calc_expr_acc_op(op);
      calc_show();
      return;
    }

  if (s_calc_typing)
    {
      double v = atof(s_calc_cur);
      if (s_calc_op)
        {
          s_calc_acc = calc_do(s_calc_acc, s_calc_op, v);
        }
      else
        {
          s_calc_acc = v;
        }
      s_calc_typing = false;
      s_calc_op = op;
      calc_expr_acc_op(op);
      calc_show();
      return;
    }

  /* consecutive operator: replace only */
  if (s_calc_op)
    {
      s_calc_op = op;
      calc_expr_acc_op(op);
      calc_show();
      return;
    }

  s_calc_acc = atof(s_calc_cur);
  s_calc_op = op;
  s_calc_typing = false;
  calc_expr_acc_op(op);
  calc_show();
}

static void calc_eval(void)
{
  double r;

  if (!s_calc_op)
    {
      s_calc_just_eq = true;
      calc_show();
      return;
    }

  if (s_calc_typing)
    {
      r = calc_do(s_calc_acc, s_calc_op, atof(s_calc_cur));
    }
  else
    {
      r = calc_do(s_calc_acc, s_calc_op, s_calc_acc);
    }

  calc_fmt(r, s_calc_cur, sizeof(s_calc_cur));
  {
    size_t n = strlen(s_calc_expr);
    snprintf(s_calc_expr + n, sizeof(s_calc_expr) - n, " %s =",
             s_calc_cur);
  }
  s_calc_acc = r;
  s_calc_op = 0;
  s_calc_typing = false;
  s_calc_just_eq = true;
  calc_show();
}

static void calc_back(void)
{
  size_t n;

  if (s_calc_just_eq)
    {
      calc_reset();
      return;
    }
  if (!s_calc_typing)
    {
      return;
    }
  n = strlen(s_calc_cur);
  if (n > 1)
    {
      s_calc_cur[n - 1] = 0;
    }
  else
    {
      snprintf(s_calc_cur, sizeof(s_calc_cur), "0");
      s_calc_typing = false;
    }
  calc_show();
}

static void calc_sign(void)
{
  if (strcmp(s_calc_cur, "0") == 0 || strcmp(s_calc_cur, "ERR") == 0)
    {
      return;
    }
  if (s_calc_cur[0] == '-')
    {
      memmove(s_calc_cur, s_calc_cur + 1, strlen(s_calc_cur));
    }
  else
    {
      char tmp[CALC_CUR_MAX];
      snprintf(tmp, sizeof(tmp), "-%s", s_calc_cur);
      snprintf(s_calc_cur, sizeof(s_calc_cur), "%s", tmp);
    }
  calc_show();
}

static void calc_key_cb(lv_event_t *e)
{
  const char *k = (const char *)lv_event_get_user_data(e);
  if (!k || !k[0])
    {
      return;
    }
  if (strcmp(k, "C") == 0)
    {
      calc_reset();
    }
  else if (strcmp(k, "BS") == 0)
    {
      calc_back();
    }
  else if (strcmp(k, "SG") == 0)
    {
      calc_sign();
    }
  else if (strcmp(k, "=") == 0)
    {
      calc_eval();
    }
  else if (k[1] == 0 && strchr("+-*/", k[0]))
    {
      calc_op(k[0]);
    }
  else if (k[1] == 0 && (k[0] == '.' || (k[0] >= '0' && k[0] <= '9')))
    {
      calc_digit(k[0]);
    }
}

static void mk_calc_btn(lv_obj_t *p, const char *text, const char *ud,
                        int x, int y, int w, int h, uint32_t bg, uint32_t fg)
{
  lv_obj_t *b = lv_button_create(p);
  lv_obj_t *t;

  lv_obj_set_size(b, w, h);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_style_radius(b, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(b, calc_key_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)ud);

  t = dm_lbl(b, text, text, g_dm_font_s, fg);
  lv_obj_center(t);
  lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);
}

void dm_create_calc(void)
{
  lv_obj_t *page = mk_tools_page(PAGE_CALC, "计算器", "Calc");
  lv_obj_t *disp;
  int row_y[5] = { 52, 88, 124, 160, 196 };
  int col_x[4] = { 8, 86, 164, 242 };
  int bw = 70;
  int bh = 30;
  int i;
  static const char *keys[5][4] = {
    { "C", "BS", "SG", "/" },
    { "7", "8", "9", "*" },
    { "4", "5", "6", "-" },
    { "1", "2", "3", "+" },
    { "0", ".", "=", NULL },
  };
  static const char *zh_op[4] = { "C", "⌫", "±", "÷" };
  static const char *zh_mul[4] = { "7", "8", "9", "×" };
  static const char *zh_sub[4] = { "4", "5", "6", "−" };

  disp = lv_obj_create(page);
  lv_obj_set_size(disp, 304, 40);
  lv_obj_set_pos(disp, 8, 34);
  lv_obj_set_style_bg_color(disp, lv_color_hex(0x0a0a0a), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(disp, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(disp, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(disp, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
  lv_obj_set_style_radius(disp, 10, LV_PART_MAIN);
  lv_obj_clear_flag(disp, LV_OBJ_FLAG_SCROLLABLE);

  s_calc_expr_l = dm_lbl(disp, " ", " ", g_dm_font_s, C_MUTED);
  lv_obj_align(s_calc_expr_l, LV_ALIGN_TOP_RIGHT, -8, 2);
  s_calc_res_l = dm_lbl(disp, "0", "0", g_dm_font_l, C_INK);
  lv_obj_align(s_calc_res_l, LV_ALIGN_BOTTOM_RIGHT, -8, -2);

  for (i = 0; i < 4; i++)
    {
      mk_calc_btn(page, zh_op[i], keys[0][i], col_x[i], row_y[0], bw, bh,
                  0x222222, 0xaaaaaa);
    }
  for (i = 0; i < 4; i++)
    {
      mk_calc_btn(page, zh_mul[i], keys[1][i], col_x[i], row_y[1], bw, bh,
                  i == 3 ? 0x0d3a4a : 0x1a1a1a, i == 3 ? C_ACCENT : C_INK);
    }
  for (i = 0; i < 4; i++)
    {
      mk_calc_btn(page, zh_sub[i], keys[2][i], col_x[i], row_y[2], bw, bh,
                  i == 3 ? 0x0d3a4a : 0x1a1a1a, i == 3 ? C_ACCENT : C_INK);
    }
  for (i = 0; i < 4; i++)
    {
      static const char *zh[4] = { "1", "2", "3", "+" };
      mk_calc_btn(page, zh[i], keys[3][i], col_x[i], row_y[3], bw, bh,
                  i == 3 ? 0x0d3a4a : 0x1a1a1a, i == 3 ? C_ACCENT : C_INK);
    }
  mk_calc_btn(page, "0", "0", col_x[0], row_y[4], bw * 2 + 8, bh, 0x1a1a1a,
              C_INK);
  mk_calc_btn(page, ".", ".", col_x[2], row_y[4], bw, bh, 0x1a1a1a, C_INK);
  mk_calc_btn(page, "=", "=", col_x[3], row_y[4], bw, bh, C_ACCENT, C_EYE);

  calc_reset();
}

/* ---------- sensors (uORB, luncher_mini style, real-time) ---------- */

static lv_obj_t *s_sen_t;
static lv_obj_t *s_sen_h;
static lv_obj_t *s_sen_l;
static int s_sen_tfd = -1;
static int s_sen_hfd = -1;
static int s_sen_lfd = -1;
static bool s_sen_inited;
static float s_sen_tv;
static float s_sen_hv;
static float s_sen_lv;
static bool s_sen_tok;
static bool s_sen_hok;
static bool s_sen_lok;

static void sen_init(void)
{
  if (s_sen_inited)
    {
      return;
    }
  s_sen_tfd = orb_subscribe_multi(ORB_ID(sensor_temp), 0);
  s_sen_hfd = orb_subscribe_multi(ORB_ID(sensor_humi), 0);
  s_sen_lfd = orb_subscribe_multi(ORB_ID(sensor_light), 0);
  if (s_sen_tfd >= 0)
    {
      orb_set_interval(s_sen_tfd, 100000);
    }
  if (s_sen_hfd >= 0)
    {
      orb_set_interval(s_sen_hfd, 100000);
    }
  if (s_sen_lfd >= 0)
    {
      orb_set_interval(s_sen_lfd, 100000);
    }
  s_sen_inited = true;
}

static void sen_label(lv_obj_t *lbl, const char *txt, uint32_t col)
{
  if (!lbl)
    {
      return;
    }
  lv_label_set_text(lbl, txt);
  lv_obj_set_style_text_color(lbl, lv_color_hex(col), LV_PART_MAIN);
}

static void sen_try_copy(void)
{
  struct pollfd fds[3];
  int n = 0;
  int ti = -1;
  int hi = -1;
  int li = -1;

  sen_init();

  if (s_sen_tfd >= 0)
    {
      ti = n;
      fds[n].fd = s_sen_tfd;
      fds[n].events = POLLIN;
      n++;
    }
  if (s_sen_hfd >= 0)
    {
      hi = n;
      fds[n].fd = s_sen_hfd;
      fds[n].events = POLLIN;
      n++;
    }
  if (s_sen_lfd >= 0)
    {
      li = n;
      fds[n].fd = s_sen_lfd;
      fds[n].events = POLLIN;
      n++;
    }
  if (n > 0)
    {
      /* short wait — real-time, do not block UI long */
      (void)poll(fds, n, 20);
    }

  if (s_sen_tfd >= 0)
    {
      struct sensor_temp d;
      if (orb_copy(ORB_ID(sensor_temp), s_sen_tfd, &d) == OK)
        {
          s_sen_tv = d.temperature;
          s_sen_tok = true;
        }
    }
  if (s_sen_hfd >= 0)
    {
      struct sensor_humi d;
      if (orb_copy(ORB_ID(sensor_humi), s_sen_hfd, &d) == OK)
        {
          s_sen_hv = d.humidity;
          s_sen_hok = true;
        }
    }
  if (s_sen_lfd >= 0)
    {
      struct sensor_light d;
      if (orb_copy(ORB_ID(sensor_light), s_sen_lfd, &d) == OK)
        {
          s_sen_lv = d.light;
          s_sen_lok = true;
        }
    }
  (void)ti;
  (void)hi;
  (void)li;
}

static void sen_paint(void)
{
  char buf[32];

  if (s_sen_tok)
    {
      snprintf(buf, sizeof(buf), "%.1f °C", (double)s_sen_tv);
      sen_label(s_sen_t, buf, C_ACCENT);
    }
  else if (s_sen_t)
    {
      sen_label(s_sen_t, "等待数据…", C_DIM);
    }

  if (s_sen_hok)
    {
      snprintf(buf, sizeof(buf), "%.0f %%", (double)s_sen_hv);
      sen_label(s_sen_h, buf, C_ACCENT);
    }
  else if (s_sen_h)
    {
      sen_label(s_sen_h, "等待数据…", C_DIM);
    }

  if (s_sen_lok)
    {
      snprintf(buf, sizeof(buf), "%.0f lx", (double)s_sen_lv);
      sen_label(s_sen_l, buf, C_ACCENT);
    }
  else if (s_sen_l)
    {
      sen_label(s_sen_l, "等待数据…", C_DIM);
    }
}

static void sen_refresh_cb(lv_event_t *e)
{
  (void)e;
  sen_try_copy();
  sen_paint();
}

static lv_obj_t *sen_card(lv_obj_t *p, const char *zh, const char *en, int y)
{
  lv_obj_t *card = lv_obj_create(p);
  lv_obj_t *lab;

  lv_obj_set_size(card, 304, 44);
  lv_obj_set_pos(card, 8, y);
  lv_obj_set_style_bg_color(card, lv_color_hex(C_BTN), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lab = dm_lbl(card, zh, en, g_dm_font_s, C_DIM);
  lv_obj_set_pos(lab, 10, 14);
  return card;
}


int dm_sensor_last_th(float *t_c, float *h_pct)
{
  sen_init();
  sen_try_copy();
  if (t_c)
    {
      *t_c = s_sen_tv;
    }
  if (h_pct)
    {
      *h_pct = s_sen_hv;
    }
  return (s_sen_tok || s_sen_hok) ? 0 : -1;
}

void dm_create_sensors(void)
{
  lv_obj_t *page = mk_tools_page(PAGE_SENSORS, "环境传感", "Sensors");
  lv_obj_t *c1;
  lv_obj_t *c2;
  lv_obj_t *c3;
  lv_obj_t *btn;
  lv_obj_t *hint;

  sen_init();

  c1 = sen_card(page, "温度 SHTC3", "Temp SHTC3", 48);
  s_sen_t = dm_lbl(c1, "等待数据…", "...", g_dm_font_l, C_DIM);
  lv_obj_align(s_sen_t, LV_ALIGN_RIGHT_MID, -10, 0);

  c2 = sen_card(page, "湿度 SHTC3", "Humi SHTC3", 100);
  s_sen_h = dm_lbl(c2, "等待数据…", "...", g_dm_font_l, C_DIM);
  lv_obj_align(s_sen_h, LV_ALIGN_RIGHT_MID, -10, 0);

  c3 = sen_card(page, "光照 LTR553", "Light LTR553", 152);
  s_sen_l = dm_lbl(c3, "等待数据…", "...", g_dm_font_l, C_DIM);
  lv_obj_align(s_sen_l, LV_ALIGN_RIGHT_MID, -10, 0);

  hint = dm_lbl(page, "实时监测中 · 每 0.25s 刷新", "Live · 0.25s",
                g_dm_font_s, C_MUTED);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 186);

  btn = dm_btn(page, "立即刷新", "Now", 100, 28, 0x0d3a4a, C_ACCENT,
               sen_refresh_cb, NULL);
  lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 206);

  sen_try_copy();
  sen_paint();
}

void dm_sensors_tick(void)
{
  if (g_dm.page != PAGE_SENSORS)
    {
      return;
    }
  /* real-time: every 250ms tick */
  sen_try_copy();
  sen_paint();
}

/* ---------- guess ---------- */

static int s_g_secret;
static int s_g_val;
static int s_g_tries;
static bool s_g_won;
static lv_obj_t *s_g_num;
static lv_obj_t *s_g_hint;
static lv_obj_t *s_g_meta;

static void guess_paint(void)
{
  char buf[8];
  char meta[32];

  snprintf(buf, sizeof(buf), "%d", s_g_val);
  if (s_g_num)
    {
      lv_label_set_text(s_g_num, buf);
    }
  if (s_g_meta)
    {
      if (s_g_won)
        {
          snprintf(meta, sizeof(meta), "用了 %d 次猜对！", s_g_tries);
        }
      else
        {
          snprintf(meta, sizeof(meta), "已猜 %d 次", s_g_tries);
        }
      lv_label_set_text(s_g_meta, meta);
    }
}

static void guess_new(void)
{
  s_g_secret = 1 + (int)(rand() % 100);
  s_g_val = 50;
  s_g_tries = 0;
  s_g_won = false;
  if (s_g_hint)
    {
      lv_label_set_text(s_g_hint, "我心里想了一个 1–100");
    }
  guess_paint();
}

static void guess_delta_cb(lv_event_t *e)
{
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  if (s_g_won)
    {
      return;
    }
  s_g_val += d;
  if (s_g_val < 1)
    {
      s_g_val = 1;
    }
  if (s_g_val > 100)
    {
      s_g_val = 100;
    }
  guess_paint();
}

static void guess_go_cb(lv_event_t *e)
{
  (void)e;
  if (s_g_won || !s_g_hint)
    {
      return;
    }
  s_g_tries++;
  if (s_g_val == s_g_secret)
    {
      s_g_won = true;
      lv_label_set_text(s_g_hint, "猜对了！真厉害～");
    }
  else if (s_g_val > s_g_secret)
    {
      lv_label_set_text(s_g_hint, "太大了，再小一点");
    }
  else
    {
      lv_label_set_text(s_g_hint, "太小了，再大一点");
    }
  guess_paint();
}

static void guess_reset_cb(lv_event_t *e)
{
  (void)e;
  guess_new();
}

void dm_create_guess(void)
{
  lv_obj_t *page = mk_tools_page(PAGE_GUESS, "猜数字", "Guess");
  lv_obj_t *b;
  int i;

  s_g_num = dm_lbl(page, "50", "50", g_dm_font_xl, C_INK);
  lv_obj_align(s_g_num, LV_ALIGN_TOP_MID, 0, 40);

  s_g_hint = dm_lbl(page, "我心里想了一个 1–100", "I picked 1-100",
                    g_dm_font_s, C_STAR);
  lv_obj_align(s_g_hint, LV_ALIGN_TOP_MID, 0, 92);

  s_g_meta = dm_lbl(page, "已猜 0 次", "0 tries", g_dm_font_s, C_MUTED);
  lv_obj_align(s_g_meta, LV_ALIGN_TOP_MID, 0, 110);

  b = dm_btn(page, "−10", "-10", 70, 32, C_BTN, C_INK, guess_delta_cb,
             (void *)(intptr_t)-10);
  lv_obj_set_pos(b, 16, 140);
  b = dm_btn(page, "−1", "-1", 70, 32, C_BTN, C_INK, guess_delta_cb,
             (void *)(intptr_t)-1);
  lv_obj_set_pos(b, 94, 140);
  b = dm_btn(page, "+1", "+1", 70, 32, C_BTN, C_INK, guess_delta_cb,
             (void *)(intptr_t)1);
  lv_obj_set_pos(b, 172, 140);
  b = dm_btn(page, "+10", "+10", 70, 32, C_BTN, C_INK, guess_delta_cb,
             (void *)(intptr_t)10);
  lv_obj_set_pos(b, 16, 180);

  b = dm_btn(page, "猜！", "Go", 138, 32, C_OK, C_EYE, guess_go_cb, NULL);
  lv_obj_set_pos(b, 94, 180);
  b = dm_btn(page, "新局", "New", 70, 32, 0x333333, C_INK, guess_reset_cb,
             NULL);
  lv_obj_set_pos(b, 240, 180);

  (void)i;
  guess_new();
}

/* ---------- convert ---------- */

#define CONV_CAT_LEN 0
#define CONV_CAT_WT  1
#define CONV_CAT_TP  2

static const char *const s_len_u[] = { "mm", "cm", "m", "km", "in", "ft" };
static const double s_len_b[] = { 0.001, 0.01, 1.0, 1000.0, 0.0254, 0.3048 };
static const char *const s_wt_u[] = { "g", "kg", "lb", "oz" };
static const double s_wt_b[] = { 0.001, 1.0, 0.45359237, 0.028349523125 };
static const char *const s_tp_u[] = { "°C", "°F", "K" };

static int s_cv_cat = CONV_CAT_LEN;
static int s_cv_if = 2;
static int s_cv_it = 1;
static double s_cv_from = 1.0;
static lv_obj_t *s_cv_from_l;
static lv_obj_t *s_cv_to_l;
static lv_obj_t *s_cv_uf;
static lv_obj_t *s_cv_ut;
static lv_obj_t *s_cv_seg[3];

static int conv_n(void)
{
  if (s_cv_cat == CONV_CAT_LEN)
    {
      return 6;
    }
  if (s_cv_cat == CONV_CAT_WT)
    {
      return 4;
    }
  return 3;
}

static const char *const *conv_units(void)
{
  if (s_cv_cat == CONV_CAT_LEN)
    {
      return s_len_u;
    }
  if (s_cv_cat == CONV_CAT_WT)
    {
      return s_wt_u;
    }
  return s_tp_u;
}

static double conv_temp(double v, int from, int to)
{
  double c = v;
  if (from == 1)
    {
      c = (v - 32.0) * 5.0 / 9.0;
    }
  else if (from == 2)
    {
      c = v - 273.15;
    }
  if (to == 1)
    {
      return c * 9.0 / 5.0 + 32.0;
    }
  if (to == 2)
    {
      return c + 273.15;
    }
  return c;
}

static double conv_calc(double v)
{
  if (s_cv_cat == CONV_CAT_TP)
    {
      return conv_temp(v, s_cv_if, s_cv_it);
    }
  if (s_cv_cat == CONV_CAT_LEN)
    {
      return v * s_len_b[s_cv_if] / s_len_b[s_cv_it];
    }
  return v * s_wt_b[s_cv_if] / s_wt_b[s_cv_it];
}

static void conv_paint(void)
{
  char buf[24];
  double out;
  const char *const *u = conv_units();

  if (s_cv_if >= conv_n())
    {
      s_cv_if = 0;
    }
  if (s_cv_it >= conv_n())
    {
      s_cv_it = 1 % conv_n();
    }
  if (s_cv_uf)
    {
      lv_label_set_text(s_cv_uf, u[s_cv_if]);
    }
  if (s_cv_ut)
    {
      lv_label_set_text(s_cv_ut, u[s_cv_it]);
    }
  snprintf(buf, sizeof(buf), "%.10g", s_cv_from);
  if (s_cv_from_l)
    {
      lv_label_set_text(s_cv_from_l, buf);
    }
  out = conv_calc(s_cv_from);
  snprintf(buf, sizeof(buf), "%.10g", out);
  if (s_cv_to_l)
    {
      lv_label_set_text(s_cv_to_l, buf);
    }
}

static void conv_from_cb(lv_event_t *e)
{
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  s_cv_from += d;
  if (s_cv_from < -1e9)
    {
      s_cv_from = -1e9;
    }
  conv_paint();
}

static void conv_cycle_uf(lv_event_t *e)
{
  (void)e;
  s_cv_if = (s_cv_if + 1) % conv_n();
  conv_paint();
}

static void conv_cycle_ut(lv_event_t *e)
{
  (void)e;
  s_cv_it = (s_cv_it + 1) % conv_n();
  conv_paint();
}

static void conv_swap(lv_event_t *e)
{
  int t;
  double out;
  (void)e;
  t = s_cv_if;
  s_cv_if = s_cv_it;
  s_cv_it = t;
  out = conv_calc(s_cv_from);
  s_cv_from = out;
  conv_paint();
}

static void conv_cat_cb(lv_event_t *e)
{
  int c = (int)(intptr_t)lv_event_get_user_data(e);
  int i;
  s_cv_cat = c;
  s_cv_if = (c == CONV_CAT_TP) ? 0 : 2;
  s_cv_it = (c == CONV_CAT_TP) ? 1 : 1;
  s_cv_from = 1.0;
  for (i = 0; i < 3; i++)
    {
      if (!s_cv_seg[i])
        {
          continue;
        }
      lv_obj_set_style_bg_color(s_cv_seg[i],
                                lv_color_hex(i == c ? 0x0d3a4a : C_BTN),
                                LV_PART_MAIN);
    }
  conv_paint();
}

void dm_create_convert(void)
{
  lv_obj_t *page = mk_tools_page(PAGE_CONVERT, "单位换算", "Convert");
  lv_obj_t *b;
  lv_obj_t *lab;
  static const char *zh_seg[3] = { "长度", "重量", "温度" };
  int i;

  for (i = 0; i < 3; i++)
    {
      b = dm_btn(page, zh_seg[i], zh_seg[i], 96, 26,
                 i == 0 ? 0x0d3a4a : C_BTN, i == 0 ? C_ACCENT : C_DIM,
                 conv_cat_cb, (void *)(intptr_t)i);
      lv_obj_set_pos(b, 8 + i * 102, 36);
      s_cv_seg[i] = b;
    }

  lab = dm_lbl(page, "从", "From", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(lab, 16, 72);
  s_cv_from_l = dm_lbl(page, "1", "1", g_dm_font_l, C_INK);
  lv_obj_set_pos(s_cv_from_l, 16, 90);
  b = dm_btn(page, "−1", "-1", 40, 28, C_BTN, C_DIM, conv_from_cb,
             (void *)(intptr_t)-1);
  lv_obj_set_pos(b, 140, 88);
  b = dm_btn(page, "+1", "+1", 40, 28, C_BTN, C_DIM, conv_from_cb,
             (void *)(intptr_t)1);
  lv_obj_set_pos(b, 188, 88);
  b = dm_btn(page, "m", "m", 48, 28, 0x222222, C_INK, conv_cycle_uf, NULL);
  lv_obj_set_pos(b, 248, 88);
  s_cv_uf = lv_obj_get_child(b, 0);

  b = dm_btn(page, "⇅", "Swap", 60, 24, C_BTN, C_ACCENT, conv_swap, NULL);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 128);

  lab = dm_lbl(page, "到", "To", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(lab, 16, 158);
  s_cv_to_l = dm_lbl(page, "100", "100", g_dm_font_l, C_ACCENT);
  lv_obj_set_pos(s_cv_to_l, 16, 176);
  b = dm_btn(page, "cm", "cm", 48, 28, 0x222222, C_INK, conv_cycle_ut, NULL);
  lv_obj_set_pos(b, 248, 174);
  s_cv_ut = lv_obj_get_child(b, 0);

  conv_paint();
}

/* ---------- entry ---------- */

void dm_create_tools(void)
{
  srand((unsigned)time(NULL));
  sen_init(); /* start uORB subscriptions early so first sample is ready */
  dm_create_calc();
  dm_create_sensors();
  dm_create_guess();
  dm_create_convert();
}

void dm_tools_tick(void)
{
  dm_sensors_tick();
}

#endif /* CONFIG_DESKMATE_APP */
