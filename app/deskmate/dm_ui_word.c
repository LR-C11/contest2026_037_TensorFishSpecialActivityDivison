/****************************************************************************
 * dm_ui_word.c — Word Memo board pages
 * Bank: g_dm_words (dm_word_bank.c, ~1100 words from wordsmemo)
 ****************************************************************************/

#include "deskmate.h"

#ifdef CONFIG_DESKMATE_APP

#include "dm_word_bank.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DM_WORD_PATH "/data/deskmate_word.bin"
#define DM_WORD_MAGIC 0x574f5244u
#define DM_STORE_MAX 1200
#define DM_ROUND 8
#define DM_QUIZ_N 5

typedef struct
{
  uint32_t magic;
  int32_t learned;
  int32_t wrong_n;
  int32_t daily_goal;
  int32_t bank_n;
  uint8_t known[DM_STORE_MAX];
  uint8_t wrong[DM_STORE_MAX];
} word_store_t;

static word_store_t s_ws;
static int s_review;
static int s_idx;
static int s_flip;
static int s_k;
static int s_u;
static int s_qi;
static int s_q_ok;
static int s_q_bad;
static int s_q_ans;
static int s_q_mode;
static int s_q_lock;
static int s_cheer_n;

static lv_obj_t *s_n_learn;
static lv_obj_t *s_n_left;
static lv_obj_t *s_s_title;
static lv_obj_t *s_s_prog;
static lv_obj_t *s_w_en;
static lv_obj_t *s_w_cn;
static lv_obj_t *s_say;
static lv_obj_t *s_r_title;
static lv_obj_t *s_r_a;
static lv_obj_t *s_r_b;
static lv_obj_t *s_r_al;
static lv_obj_t *s_r_bl;
static lv_obj_t *s_r_p;
static lv_obj_t *s_r_msg;
static lv_obj_t *s_q_word;
static lv_obj_t *s_q_sub;
static lv_obj_t *s_q_prog;
static lv_obj_t *s_q_opts[4];
static lv_obj_t *s_q_next;
static lv_obj_t *s_wrong_box;
static lv_obj_t *s_list_box;
static lv_obj_t *s_day_l;

static int bank_n(void)
{
  if (g_dm_words_n < 1)
    {
      return 0;
    }
  if (g_dm_words_n > DM_STORE_MAX)
    {
      return DM_STORE_MAX;
    }
  return g_dm_words_n;
}

static void store_save(void)
{
  int fd = open(DM_WORD_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  s_ws.magic = DM_WORD_MAGIC;
  s_ws.bank_n = bank_n();
  if (fd >= 0)
    {
      (void)write(fd, &s_ws, sizeof(s_ws));
      close(fd);
    }
}

static void store_load(void)
{
  int fd;
  word_store_t t;

  memset(&s_ws, 0, sizeof(s_ws));
  s_ws.daily_goal = 10;
  fd = open(DM_WORD_PATH, O_RDONLY);
  if (fd >= 0)
    {
      if (read(fd, &t, sizeof(t)) == (ssize_t)sizeof(t) &&
          t.magic == DM_WORD_MAGIC)
        {
          s_ws = t;
        }
      close(fd);
    }
}

static void recount(void)
{
  int n = bank_n();
  int i;
  s_ws.learned = 0;
  s_ws.wrong_n = 0;
  for (i = 0; i < n; i++)
    {
      if (s_ws.known[i])
        {
          s_ws.learned++;
        }
      if (s_ws.wrong[i])
        {
          s_ws.wrong_n++;
        }
    }
}

static lv_obj_t *mk_page(dm_page_t id)
{
  lv_obj_t *p = lv_obj_create(g_dm_root);
  lv_obj_set_size(p, DM_SCR_W, DM_SCR_H);
  lv_obj_set_style_bg_color(p, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(p, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(p, 0, LV_PART_MAIN);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[id] = p;
  return p;
}

static lv_obj_t *sc_body(lv_obj_t *page)
{
  lv_obj_t *sc = lv_obj_create(page);
  lv_obj_set_size(sc, DM_SCR_W, 160);
  lv_obj_set_pos(sc, 0, 28);
  lv_obj_set_style_bg_opa(sc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(sc, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_row(sc, 6, LV_PART_MAIN);
  lv_obj_set_scroll_dir(sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(sc, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(sc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  return sc;
}

static void add_title(lv_obj_t *page, const char *zh, const char *en,
                      lv_event_cb_t back)
{
  lv_obj_t *t = dm_lbl(page, zh, en, g_dm_font_m, C_INK);
  lv_obj_set_pos(t, 48, 10);
  lv_obj_t *b = dm_btn(page, "←", "<", 36, 24, C_BTN, C_MUTED, back, NULL);
  lv_obj_set_pos(b, 8, 8);
}

static lv_obj_t *nav_row(lv_obj_t *sc, const char *zh, const char *en,
                         const char *hzh, const char *hen, lv_event_cb_t cb)
{
  lv_obj_t *c = lv_obj_create(sc);
  lv_obj_t *n;
  lv_obj_t *h;
  lv_obj_t *a;

  lv_obj_set_size(c, 292, 44);
  lv_obj_set_style_bg_color(c, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 10, LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 8, LV_PART_MAIN);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, NULL);
  n = dm_lbl(c, zh, en, g_dm_font_s, C_INK);
  lv_obj_set_pos(n, 0, 2);
  h = dm_lbl(c, hzh, hen, g_dm_font_s, C_MUTED);
  lv_obj_set_pos(h, 0, 20);
  a = dm_lbl(c, ">", ">", g_dm_font_m, C_MUTED);
  lv_obj_align(a, LV_ALIGN_RIGHT_MID, -4, 0);
  return c;
}

static void refresh_home(void)
{
  char b[24];
  int n = bank_n();
  recount();
  if (s_n_learn)
    {
      lv_snprintf(b, sizeof(b), "%ld", (long)s_ws.learned);
      lv_label_set_text(s_n_learn, b);
    }
  if (s_n_left)
    {
      lv_snprintf(b, sizeof(b), "%ld", (long)(n - s_ws.learned));
      lv_label_set_text(s_n_left, b);
    }
}

static void cb_features(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_FEATURES);
}

static void cb_word_home(lv_event_t *e)
{
  (void)e;
  refresh_home();
  dm_show(PAGE_WORD_HOME);
}

static void show_card(void)
{
  char b[16];
  int n = bank_n();
  if (s_idx < 0 || s_idx >= n)
    {
      s_idx = 0;
    }
  if (s_w_en)
    {
      lv_label_set_text(s_w_en, g_dm_words[s_idx].en);
    }
  if (s_w_cn)
    {
      lv_label_set_text(s_w_cn,
                        s_flip ? g_dm_words[s_idx].cn
                               : dm_t("点卡片看释义", "Tap for meaning"));
    }
  if (s_s_prog)
    {
      lv_snprintf(b, sizeof(b), "%d/%d", s_idx + 1, DM_ROUND);
      lv_label_set_text(s_s_prog, b);
    }
}

static void mascot_cheer(int good)
{
  static const char *ok_zh[] = { "太棒了！继续加油～", "爱你哟！记住它啦",
                                 "答对啦，真聪明！" };
  static const char *ok_en[] = { "Great! Keep going~", "Love it! You got it",
                                 "Correct! Smart~" };
  static const char *no_zh[] = { "别灰心，再看一眼就好～", "没关系，下次一定行",
                                 "慢慢来，我在陪你" };
  static const char *no_en[] = { "No worries, look again~",
                                 "It's okay, next time",
                                 "Take your time, I'm here" };

  s_cheer_n++;
  if (good)
    {
      if ((s_cheer_n % 3) == 2)
        {
          dm_face_set(FACE_LOVE, EYE_DECOR_HEART, 4);
        }
      else if ((s_cheer_n % 3) == 0)
        {
          dm_face_set(FACE_LAUGH, EYE_DECOR_STAR, 4);
        }
      else
        {
          dm_face_set(FACE_HAPPY, EYE_DECOR_STAR, 4);
        }
      if (s_say)
        {
          lv_label_set_text(s_say,
                            dm_t(ok_zh[s_cheer_n % 3], ok_en[s_cheer_n % 3]));
        }
    }
  else
    {
      dm_face_set(FACE_CRY, EYE_DECOR_NONE, 4);
      if (s_say)
        {
          lv_label_set_text(s_say,
                            dm_t(no_zh[s_cheer_n % 3], no_en[s_cheer_n % 3]));
        }
    }
}

static void cb_flip(lv_event_t *e)
{
  (void)e;
  s_flip = !s_flip;
  show_card();
}

static void cb_mark(lv_event_t *e)
{
  int known = (int)(uintptr_t)lv_event_get_user_data(e);
  char b[16];
  int acc;
  int n = bank_n();

  if (known)
    {
      s_k++;
      s_ws.known[s_idx] = 1;
      s_ws.wrong[s_idx] = 0;
      mascot_cheer(1);
    }
  else
    {
      s_u++;
      s_ws.wrong[s_idx] = 1;
      mascot_cheer(0);
    }
  s_idx++;
  s_flip = 0;
  if (s_idx < DM_ROUND && s_idx < n)
    {
      show_card();
      return;
    }
  recount();
  store_save();
  if (s_r_title)
    {
      lv_label_set_text(s_r_title,
                        s_review ? dm_t("复习完成", "Review done")
                                 : dm_t("学习完成", "Study done"));
    }
  if (s_r_al)
    {
      lv_label_set_text(s_r_al, dm_t("认识", "Know"));
    }
  if (s_r_bl)
    {
      lv_label_set_text(s_r_bl, dm_t("不认识", "Unknown"));
    }
  if (s_r_a)
    {
      lv_snprintf(b, sizeof(b), "%d", s_k);
      lv_label_set_text(s_r_a, b);
    }
  if (s_r_b)
    {
      lv_snprintf(b, sizeof(b), "%d", s_u);
      lv_label_set_text(s_r_b, b);
    }
  acc = (s_k + s_u) ? (s_k * 100 / (s_k + s_u)) : 0;
  if (s_r_p)
    {
      lv_snprintf(b, sizeof(b), "%d%%", acc);
      lv_label_set_text(s_r_p, b);
    }
  if (s_r_msg)
    {
      lv_label_set_text(s_r_msg,
                        acc >= 90 ? dm_t("太棒了！", "Awesome!")
                                  : (acc >= 70 ? dm_t("不错，继续", "Keep going")
                                               : dm_t("再巩固一下",
                                                      "Drill hard words")));
    }
  dm_show(PAGE_WORD_RES);
}

static void start_round(int review)
{
  int n = bank_n();
  s_review = review;
  s_idx = (review && s_ws.learned > 0) ? (s_cheer_n % (n > 0 ? n : 1)) : 0;
  s_flip = 0;
  s_k = 0;
  s_u = 0;
  if (s_s_title)
    {
      lv_label_set_text(s_s_title,
                        review ? dm_t("复习", "Review")
                               : dm_t("学习", "Study"));
    }
  show_card();
  dm_show(PAGE_WORD_STUDY);
  if (g_dm_pages[PAGE_WORD_STUDY])
    {
      dm_face_attach(g_dm_pages[PAGE_WORD_STUDY], (DM_SCR_W - 52) / 2, 30);
      if (g_dm_face)
        {
          lv_obj_set_size(g_dm_face, 52, 52);
        }
    }
  if (s_say)
    {
      lv_label_set_text(s_say, dm_t("我们一起记，点卡片看释义",
                                    "Let's learn — tap the card"));
    }
  dm_face_set(FACE_IDLE, EYE_DECOR_NONE, 0);
}

static void cb_study(lv_event_t *e)
{
  (void)e;
  start_round(0);
}

static void cb_review(lv_event_t *e)
{
  (void)e;
  start_round(1);
}

static void show_quiz(void)
{
  char opts[4][28];
  int n = bank_n();
  int i;
  int shift;

  s_q_lock = 0;
  if (s_q_next)
    {
      lv_obj_add_flag(s_q_next, LV_OBJ_FLAG_HIDDEN);
    }
  if (s_q_prog)
    {
      char b[12];
      lv_snprintf(b, sizeof(b), "%d/%d", s_qi + 1, DM_QUIZ_N);
      lv_label_set_text(s_q_prog, b);
    }
  if (s_q_mode == 0)
    {
      if (s_q_word)
        {
          lv_label_set_text(s_q_word, g_dm_words[s_qi].en);
        }
      if (s_q_sub)
        {
          lv_label_set_text(s_q_sub, dm_t("选择释义", "Pick meaning"));
        }
      strncpy(opts[0], g_dm_words[s_qi].cn, 27);
    }
  else
    {
      if (s_q_word)
        {
          lv_label_set_text(s_q_word, g_dm_words[s_qi].cn);
        }
      if (s_q_sub)
        {
          lv_label_set_text(s_q_sub, dm_t("选择单词", "Pick word"));
        }
      strncpy(opts[0], g_dm_words[s_qi].en, 27);
    }
  opts[0][27] = 0;
  for (i = 1; i < 4; i++)
    {
      int j = (s_qi + i * 7) % n;
      strncpy(opts[i], s_q_mode == 0 ? g_dm_words[j].cn : g_dm_words[j].en,
              27);
      opts[i][27] = 0;
    }
  shift = s_qi % 4;
  s_q_ans = (4 - shift) % 4;
  for (i = 0; i < 4; i++)
    {
      int src = (i + shift) % 4;
      lv_obj_t *lab;
      if (!s_q_opts[i])
        {
          continue;
        }
      lv_obj_set_style_bg_color(s_q_opts[i], lv_color_hex(C_BTN_HI),
                                LV_PART_MAIN);
      lab = lv_obj_get_child(s_q_opts[i], 0);
      if (lab)
        {
          lv_label_set_text(lab, opts[src]);
          lv_obj_set_style_text_color(lab, lv_color_hex(C_INK), LV_PART_MAIN);
        }
    }
}

static void cb_qopt(lv_event_t *e)
{
  int pick = (int)(uintptr_t)lv_event_get_user_data(e);
  int i;

  if (s_q_lock)
    {
      return;
    }
  s_q_lock = 1;
  for (i = 0; i < 4; i++)
    {
      lv_obj_t *lab;
      uint32_t bg = C_BTN_HI;
      uint32_t fg = C_INK;
      if (i == s_q_ans)
        {
          bg = C_OK;
          fg = C_EYE;
        }
      else if (i == pick)
        {
          bg = C_HEART;
          fg = C_EYE;
        }
      if (!s_q_opts[i])
        {
          continue;
        }
      lv_obj_set_style_bg_color(s_q_opts[i], lv_color_hex(bg), LV_PART_MAIN);
      lab = lv_obj_get_child(s_q_opts[i], 0);
      if (lab)
        {
          lv_obj_set_style_text_color(lab, lv_color_hex(fg), LV_PART_MAIN);
        }
    }
  if (pick == s_q_ans)
    {
      s_q_ok++;
      mascot_cheer(1);
    }
  else
    {
      s_q_bad++;
      s_ws.wrong[s_qi % bank_n()] = 1;
      mascot_cheer(0);
    }
  if (s_q_next)
    {
      lv_obj_clear_flag(s_q_next, LV_OBJ_FLAG_HIDDEN);
    }
}

static void cb_qnext(lv_event_t *e)
{
  char b[16];
  int acc;
  (void)e;

  s_qi++;
  if (s_qi < DM_QUIZ_N)
    {
      show_quiz();
      return;
    }
  store_save();
  if (s_r_title)
    {
      lv_label_set_text(s_r_title, dm_t("测验完成", "Quiz done"));
    }
  if (s_r_al)
    {
      lv_label_set_text(s_r_al, dm_t("正确", "OK"));
    }
  if (s_r_bl)
    {
      lv_label_set_text(s_r_bl, dm_t("错误", "Bad"));
    }
  if (s_r_a)
    {
      lv_snprintf(b, sizeof(b), "%d", s_q_ok);
      lv_label_set_text(s_r_a, b);
    }
  if (s_r_b)
    {
      lv_snprintf(b, sizeof(b), "%d", s_q_bad);
      lv_label_set_text(s_r_b, b);
    }
  acc = s_q_ok * 100 / DM_QUIZ_N;
  if (s_r_p)
    {
      lv_snprintf(b, sizeof(b), "%d%%", acc);
      lv_label_set_text(s_r_p, b);
    }
  if (s_r_msg)
    {
      lv_label_set_text(s_r_msg,
                        s_q_ok >= 4 ? dm_t("成绩很好！", "Great!")
                                    : dm_t("错词进错题本", "Check wrong book"));
    }
  dm_show(PAGE_WORD_RES);
}

static void cb_qmode(lv_event_t *e)
{
  s_q_mode = (int)(uintptr_t)lv_event_get_user_data(e);
  s_qi = 0;
  s_q_ok = 0;
  s_q_bad = 0;
  show_quiz();
  dm_show(PAGE_WORD_QUIZ);
  if (g_dm_pages[PAGE_WORD_QUIZ])
    {
      dm_face_attach(g_dm_pages[PAGE_WORD_QUIZ], (DM_SCR_W - 40) / 2, 28);
      if (g_dm_face)
        {
          lv_obj_set_size(g_dm_face, 40, 40);
        }
    }
  if (s_say)
    {
      lv_label_set_text(s_say, "");
    }
  dm_face_set(FACE_IDLE, EYE_DECOR_NONE, 0);
}

static void fill_wrong(void)
{
  int n = bank_n();
  int i;
  if (!s_wrong_box)
    {
      return;
    }
  lv_obj_clean(s_wrong_box);
  recount();
  if (s_ws.wrong_n == 0)
    {
      lv_obj_t *t = dm_lbl(s_wrong_box, "暂无错词", "Empty", g_dm_font_s,
                           C_MUTED);
      lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(t, 260);
      return;
    }
  for (i = 0; i < n; i++)
    {
      char line[48];
      lv_obj_t *t;
      if (!s_ws.wrong[i])
        {
          continue;
        }
      lv_snprintf(line, sizeof(line), "%s · %s", g_dm_words[i].en,
                  g_dm_words[i].cn);
      t = dm_lbl(s_wrong_box, line, line, g_dm_font_s, C_INK);
      lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
      lv_obj_set_width(t, 260);
    }
}

static void cb_wrong(lv_event_t *e)
{
  (void)e;
  fill_wrong();
  dm_show(PAGE_WORD_WRONG);
}

static void fill_list(void)
{
  int n = bank_n();
  int i;
  if (!s_list_box)
    {
      return;
    }
  lv_obj_clean(s_list_box);
  for (i = 0; i < n; i++)
    {
      char line[48];
      lv_obj_t *t;
      lv_snprintf(line, sizeof(line), "%s %s",
                  s_ws.known[i] ? "*" : "-", g_dm_words[i].en);
      t = dm_lbl(s_list_box, line, line, g_dm_font_s,
                 s_ws.known[i] ? C_OK : C_INK);
      lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
      lv_obj_set_width(t, 260);
    }
}

static void cb_list(lv_event_t *e)
{
  (void)e;
  fill_list();
  dm_show(PAGE_WORD_LIST);
}

static void cb_day_minus(lv_event_t *e)
{
  char b[16];
  (void)e;
  if (s_ws.daily_goal > 5)
    {
      s_ws.daily_goal--;
      store_save();
    }
  if (s_day_l)
    {
      lv_snprintf(b, sizeof(b), "%ld", (long)s_ws.daily_goal);
      lv_label_set_text(s_day_l, b);
    }
}

static void cb_day_plus(lv_event_t *e)
{
  char b[16];
  (void)e;
  if (s_ws.daily_goal < 50)
    {
      s_ws.daily_goal++;
      store_save();
    }
  if (s_day_l)
    {
      lv_snprintf(b, sizeof(b), "%ld", (long)s_ws.daily_goal);
      lv_label_set_text(s_day_l, b);
    }
}

static void cb_wset(lv_event_t *e)
{
  char b[16];
  (void)e;
  if (s_day_l)
    {
      lv_snprintf(b, sizeof(b), "%ld", (long)s_ws.daily_goal);
      lv_label_set_text(s_day_l, b);
    }
  dm_show(PAGE_WORD_WSET);
}

void dm_create_word(void)
{
  lv_obj_t *page;
  lv_obj_t *sc;
  lv_obj_t *card;
  int i;
  char leftb[24];

  store_load();

  page = mk_page(PAGE_WORD_HOME);
  add_title(page, "背单词", "Words", cb_features);
  sc = sc_body(page);

  card = lv_obj_create(sc);
  lv_obj_set_size(card, 292, 78);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  {
    lv_obj_t *h = dm_lbl(card, "学习进度", "Progress", g_dm_font_s, C_MUTED);
    lv_obj_set_pos(h, 0, 0);
    s_n_learn = dm_lbl(card, "0", "0", g_dm_font_l, C_INK);
    lv_obj_set_pos(s_n_learn, 10, 20);
    {
      lv_obj_t *l = dm_lbl(card, "已学", "Learned", g_dm_font_s, C_MUTED);
      lv_obj_set_pos(l, 10, 50);
    }
    lv_snprintf(leftb, sizeof(leftb), "%d", bank_n());
    s_n_left = dm_lbl(card, leftb, leftb, g_dm_font_l, C_INK);
    lv_obj_set_pos(s_n_left, 110, 20);
    {
      lv_obj_t *l = dm_lbl(card, "未学", "Left", g_dm_font_s, C_MUTED);
      lv_obj_set_pos(l, 110, 50);
    }
  }

  nav_row(sc, "开始学习", "Study", "新词卡片", "New", cb_study);
  nav_row(sc, "开始复习", "Review", "回顾旧词", "Old", cb_review);
  nav_row(sc, "错题本", "Wrong", "未掌握", "Hard", cb_wrong);
  nav_row(sc, "词库浏览", "List", "内置词库", "Bank", cb_list);
  nav_row(sc, "单词设定", "Settings", "每日目标", "Daily goal", cb_wset);

  {
    lv_obj_t *qc = lv_obj_create(sc);
    lv_obj_set_size(qc, 292, 60);
    lv_obj_set_style_bg_color(qc, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(qc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(qc, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(qc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(qc, 8, LV_PART_MAIN);
    lv_obj_clear_flag(qc, LV_OBJ_FLAG_SCROLLABLE);
    {
      lv_obj_t *h = dm_lbl(qc, "自我测验", "Quiz", g_dm_font_s, C_INK);
      lv_obj_set_pos(h, 0, 0);
      lv_obj_t *b1 = dm_btn(qc, "英译中", "En→Cn", 120, 26, C_BTN_HI, C_INK,
                            cb_qmode, (void *)(uintptr_t)0);
      lv_obj_set_pos(b1, 0, 26);
      lv_obj_t *b2 = dm_btn(qc, "中译英", "Cn→En", 120, 26, C_BTN_HI, C_INK,
                            cb_qmode, (void *)(uintptr_t)1);
      lv_obj_set_pos(b2, 150, 26);
    }
  }
  refresh_home();

  page = mk_page(PAGE_WORD_STUDY);
  add_title(page, "学习", "Study", cb_word_home);
  s_s_title = dm_lbl(page, "学习", "Study", g_dm_font_s, C_DIM);
  lv_obj_set_pos(s_s_title, 120, 12);
  s_s_prog = dm_lbl(page, "1/8", "1/8", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_s_prog, 270, 12);
  s_say = dm_lbl(page, "", "", g_dm_font_s, C_INK);
  lv_obj_set_style_text_align(s_say, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(s_say, 300);
  lv_obj_align(s_say, LV_ALIGN_TOP_MID, 0, 88);
  card = lv_obj_create(page);
  lv_obj_set_size(card, 292, 78);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 108);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, cb_flip, LV_EVENT_CLICKED, NULL);
  s_w_en = dm_lbl(card, "word", "word", g_dm_font_l, C_INK);
  lv_obj_set_style_text_align(s_w_en, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_w_en, LV_ALIGN_TOP_MID, 0, 16);
  s_w_cn = dm_lbl(card, "tap", "tap", g_dm_font_s, C_DIM);
  lv_obj_set_style_text_align(s_w_cn, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_w_cn, LV_ALIGN_TOP_MID, 0, 50);
  {
    lv_obj_t *b1 = dm_btn(page, "不认识", "No", 130, 32, C_HEART, C_EYE,
                          cb_mark, (void *)(uintptr_t)0);
    lv_obj_set_pos(b1, 24, 196);
    lv_obj_t *b2 = dm_btn(page, "认识", "Yes", 130, 32, C_OK, C_EYE, cb_mark,
                          (void *)(uintptr_t)1);
    lv_obj_set_pos(b2, 166, 196);
  }

  page = mk_page(PAGE_WORD_RES);
  s_r_title = dm_lbl(page, "完成", "Done", g_dm_font_m, C_INK);
  lv_obj_align(s_r_title, LV_ALIGN_TOP_MID, 0, 18);
  s_r_a = dm_lbl(page, "0", "0", g_dm_font_l, C_OK);
  lv_obj_set_pos(s_r_a, 40, 60);
  s_r_al = dm_lbl(page, "认识", "OK", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_r_al, 40, 96);
  s_r_b = dm_lbl(page, "0", "0", g_dm_font_l, C_HEART);
  lv_obj_set_pos(s_r_b, 140, 60);
  s_r_bl = dm_lbl(page, "不认识", "No", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_r_bl, 140, 96);
  s_r_p = dm_lbl(page, "0%", "0%", g_dm_font_l, C_INK);
  lv_obj_set_pos(s_r_p, 230, 60);
  {
    lv_obj_t *l = dm_lbl(page, "正确率", "Acc", g_dm_font_s, C_MUTED);
    lv_obj_set_pos(l, 230, 96);
  }
  s_r_msg = dm_lbl(page, "", "", g_dm_font_s, C_DIM);
  lv_obj_align(s_r_msg, LV_ALIGN_TOP_MID, 0, 128);
  {
    lv_obj_t *b1 = dm_btn(page, "返回", "Back", 100, 30, C_BTN, C_MUTED,
                          cb_word_home, NULL);
    lv_obj_set_pos(b1, 50, 172);
    lv_obj_t *b2 = dm_btn(page, "再来", "Again", 100, 30, C_FACE, C_EYE,
                          cb_study, NULL);
    lv_obj_set_pos(b2, 170, 172);
  }

  page = mk_page(PAGE_WORD_QUIZ);
  add_title(page, "测验", "Quiz", cb_word_home);
  s_q_prog = dm_lbl(page, "1/5", "1/5", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_q_prog, 270, 12);
  s_q_word = dm_lbl(page, "w", "w", g_dm_font_m, C_INK);
  lv_obj_set_style_text_align(s_q_word, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_q_word, LV_ALIGN_TOP_MID, 0, 76);
  s_q_sub = dm_lbl(page, "pick", "pick", g_dm_font_s, C_DIM);
  lv_obj_set_style_text_align(s_q_sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_q_sub, LV_ALIGN_TOP_MID, 0, 98);
  s_say = dm_lbl(page, "", "", g_dm_font_s, C_DIM);
  lv_obj_set_style_text_align(s_say, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(s_say, 300);
  lv_obj_align(s_say, LV_ALIGN_TOP_MID, 0, 66);
  for (i = 0; i < 4; i++)
    {
      s_q_opts[i] = dm_btn(page, "", "", 280, 26, C_BTN_HI, C_INK, cb_qopt,
                           (void *)(uintptr_t)i);
      lv_obj_set_pos(s_q_opts[i], 20, 118 + i * 28);
    }
  s_q_next = dm_btn(page, "下一题", "Next", 120, 26, C_FACE, C_EYE, cb_qnext,
                    NULL);
  lv_obj_align(s_q_next, LV_ALIGN_TOP_MID, 0, 214);
  lv_obj_add_flag(s_q_next, LV_OBJ_FLAG_HIDDEN);

  page = mk_page(PAGE_WORD_WRONG);
  add_title(page, "错题本", "Wrong", cb_word_home);
  s_wrong_box = sc_body(page);

  page = mk_page(PAGE_WORD_LIST);
  add_title(page, "词库", "List", cb_word_home);
  s_list_box = sc_body(page);

  page = mk_page(PAGE_WORD_WSET);
  add_title(page, "单词设定", "Settings", cb_word_home);
  sc = sc_body(page);
  card = lv_obj_create(sc);
  lv_obj_set_size(card, 292, 52);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(card, 8, LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  {
    lv_obj_t *n = dm_lbl(card, "每日目标", "Daily", g_dm_font_s, C_INK);
    lv_obj_set_pos(n, 0, 16);
    lv_obj_t *m = dm_btn(card, "-", "-", 32, 30, C_BTN_HI, C_INK,
                         cb_day_minus, NULL);
    lv_obj_set_pos(m, 150, 10);
    s_day_l = dm_lbl(card, "10", "10", g_dm_font_m, C_INK);
    lv_obj_set_pos(s_day_l, 192, 18);
    lv_obj_t *p = dm_btn(card, "+", "+", 32, 30, C_BTN_HI, C_INK,
                         cb_day_plus, NULL);
    lv_obj_set_pos(p, 230, 10);
  }
}

#endif /* CONFIG_DESKMATE_APP */
