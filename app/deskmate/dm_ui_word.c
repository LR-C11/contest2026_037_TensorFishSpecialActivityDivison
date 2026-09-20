/****************************************************************************
 * dm_ui_word.c — Word Memo board pages
 * Bank: g_dm_words (dm_word_bank.c, ~1100 words from wordsmemo)
 ****************************************************************************/

#include "deskmate.h"

#ifdef CONFIG_DESKMATE_APP

#include "dm_word_bank.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define DM_WORD_PATH "/data/deskmate_word.bin"
#define DM_WORD_MAGIC 0x574f5244u
#define DM_STORE_MAX 1200
#define DM_ROUND_MAX 50
#define DM_QUIZ_N 5

typedef struct
{
  uint32_t magic;
  int32_t learned;
  int32_t today_learn;
  int32_t today_key;
  int32_t wrong_n;
  int32_t daily_goal;
  int32_t bank_n;
  uint8_t known[DM_STORE_MAX];
  uint8_t wrong[DM_STORE_MAX];
} word_store_t;

static word_store_t s_ws;
static int s_review;
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

static int s_round_ids[DM_ROUND_MAX];
static int s_round_n;
static int s_round_pos;
static int s_quiz_ids[DM_QUIZ_N];
static int s_quiz_n;

static lv_obj_t *s_n_learn;
static lv_obj_t *s_n_left;
static lv_obj_t *s_s_title;
static lv_obj_t *s_s_prog;
static lv_obj_t *s_w_en;
static lv_obj_t *s_w_cn;
static lv_obj_t *s_say;
static lv_obj_t *s_say_q;

static void say_word(const char *zh, const char *en)
{
  const char *t = dm_t(zh, en);
  if (s_say)
    {
      lv_label_set_text(s_say, t);
    }
  if (s_say_q)
    {
      lv_label_set_text(s_say_q, t);
    }
}
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
          {
            time_t tw = time(NULL);
            struct tm tmw;
            int key;
            localtime_r(&tw, &tmw);
            key = tmw.tm_yday + tmw.tm_year * 1000;
            if (s_ws.today_key != key)
              {
                s_ws.today_key = key;
                s_ws.today_learn = 0;
              }
            s_ws.today_learn++;
          }
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

static int cur_word(void)
{
  if (s_round_pos < 0 || s_round_pos >= s_round_n)
    {
      return 0;
    }
  return s_round_ids[s_round_pos];
}

static void show_card(void)
{
  char b[16];
  int wi = cur_word();

  if (s_w_en)
    {
      lv_label_set_text(s_w_en, g_dm_words[wi].en);
    }
  if (s_w_cn)
    {
      lv_label_set_text(s_w_cn,
                        s_flip ? g_dm_words[wi].cn
                               : dm_t("点卡片看释义", "Tap for meaning"));
    }
  if (s_s_prog)
    {
      lv_snprintf(b, sizeof(b), "%d/%d", s_round_pos + 1, s_round_n);
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
      /* keep faces simple on small word-page mascot */
      if ((s_cheer_n % 3) == 2)
        {
          dm_face_set(FACE_LOVE, EYE_DECOR_HEART, 5);
        }
      else if ((s_cheer_n % 3) == 0)
        {
          dm_face_set(FACE_HAPPY, EYE_DECOR_STAR, 5);
        }
      else
        {
          dm_face_set(FACE_HAPPY, EYE_DECOR_NONE, 0);
        }
      if (s_say)
        {
          lv_label_set_text(s_say,
                            dm_t(ok_zh[s_cheer_n % 3], ok_en[s_cheer_n % 3]));
        }
      if (s_say_q)
        {
          lv_label_set_text(s_say_q,
                            dm_t(ok_zh[s_cheer_n % 3], ok_en[s_cheer_n % 3]));
        }
    }
  else
    {
      dm_face_set(FACE_CRY, EYE_DECOR_NONE, 0);
      if (s_say)
        {
          lv_label_set_text(s_say,
                            dm_t(no_zh[s_cheer_n % 3], no_en[s_cheer_n % 3]));
        }
      if (s_say_q)
        {
          lv_label_set_text(s_say_q,
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
  int wi = cur_word();

  if (known)
    {
      s_k++;
      s_ws.known[wi] = 1;
      s_ws.wrong[wi] = 0;
      mascot_cheer(1);
    }
  else
    {
      s_u++;
      s_ws.wrong[wi] = 1;
      mascot_cheer(0);
    }
  s_round_pos++;
  s_flip = 0;
  if (s_round_pos < s_round_n)
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

static void shuffle_pick(int *out, int *outn, int want_known)
{
  int cand[DM_STORE_MAX];
  int n = bank_n();
  int cn = 0;
  int i;
  int goal = s_ws.daily_goal;
  int want;

  if (goal < 5)
    {
      goal = 5;
    }
  if (goal > DM_ROUND_MAX)
    {
      goal = DM_ROUND_MAX;
    }

  for (i = 0; i < n; i++)
    {
      if (want_known)
        {
          if (s_ws.known[i])
            {
              cand[cn++] = i;
            }
        }
      else if (!s_ws.known[i])
        {
          cand[cn++] = i;
        }
    }
  /* fallback: if filter empty, use whole bank */
  if (cn == 0)
    {
      for (i = 0; i < n; i++)
        {
          cand[cn++] = i;
        }
    }
  /* Fisher-Yates */
  for (i = cn - 1; i > 0; i--)
    {
      int j = (int)(rand() % (i + 1));
      int t = cand[i];
      cand[i] = cand[j];
      cand[j] = t;
    }
  want = goal < cn ? goal : cn;
  for (i = 0; i < want; i++)
    {
      out[i] = cand[i];
    }
  *outn = want;
}

static void start_round(int review)
{
  s_review = review;
  s_flip = 0;
  s_k = 0;
  s_u = 0;
  s_round_pos = 0;
  s_round_n = 0;
  shuffle_pick(s_round_ids, &s_round_n, review ? 1 : 0);
  if (s_round_n == 0)
    {
      s_round_n = 1;
      s_round_ids[0] = 0;
    }
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
      dm_face_attach(g_dm_pages[PAGE_WORD_STUDY], (DM_SCR_W - 72) / 2, 34);
      if (g_dm_face)
        {
          lv_obj_set_size(g_dm_face, 72, 72);
        }
    }
  if (s_say)
    {
      lv_label_set_text(s_say, dm_t("我们一起记，点卡片看释义",
                                    "Let's learn — tap the card"));
    }
  if (s_say_q)
    {
      lv_label_set_text(s_say_q, dm_t("我们一起记，点卡片看释义",
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
  int wi;

  s_q_lock = 0;
  if (s_qi < 0 || s_qi >= s_quiz_n)
    {
      return;
    }
  wi = s_quiz_ids[s_qi];
  if (wi < 0 || wi >= n)
    {
      wi = 0;
    }
  if (s_q_next)
    {
      lv_obj_add_flag(s_q_next, LV_OBJ_FLAG_HIDDEN);
    }
  if (s_q_prog)
    {
      char b[12];
      lv_snprintf(b, sizeof(b), "%d/%d", s_qi + 1, s_quiz_n);
      lv_label_set_text(s_q_prog, b);
    }
  if (s_q_mode == 0)
    {
      if (s_q_word)
        {
          lv_label_set_text(s_q_word, g_dm_words[wi].en);
        }
      if (s_q_sub)
        {
          lv_label_set_text(s_q_sub, dm_t("选择释义", "Pick meaning"));
        }
      strncpy(opts[0], g_dm_words[wi].cn, 27);
    }
  else
    {
      if (s_q_word)
        {
          lv_label_set_text(s_q_word, g_dm_words[wi].cn);
        }
      if (s_q_sub)
        {
          lv_label_set_text(s_q_sub, dm_t("选择单词", "Pick word"));
        }
      strncpy(opts[0], g_dm_words[wi].en, 27);
    }
  opts[0][27] = 0;
  for (i = 1; i < 4; i++)
    {
      int j = (wi + i * 37 + 11) % n;
      if (j == wi)
        {
          j = (j + 1) % n;
        }
      strncpy(opts[i], s_q_mode == 0 ? g_dm_words[j].cn : g_dm_words[j].en,
              27);
      opts[i][27] = 0;
    }
  shift = wi % 4;
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
      int wi = (s_qi >= 0 && s_qi < s_quiz_n) ? s_quiz_ids[s_qi] : 0;
      s_q_bad++;
      if (wi >= 0 && wi < bank_n())
        {
          s_ws.wrong[wi] = 1;
        }
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
  if (s_qi < s_quiz_n)
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
  acc = s_quiz_n ? (s_q_ok * 100 / s_quiz_n) : 0;
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
  int n = bank_n();
  int i;
  s_q_mode = (int)(uintptr_t)lv_event_get_user_data(e);
  s_qi = 0;
  s_q_ok = 0;
  s_q_bad = 0;
  s_quiz_n = DM_QUIZ_N;
  if (n < s_quiz_n)
    {
      s_quiz_n = n > 0 ? n : 1;
    }
  for (i = 0; i < s_quiz_n; i++)
    {
      s_quiz_ids[i] = n > 0 ? (int)(rand() % n) : 0;
    }
  /* avoid immediate duplicates */
  for (i = 1; i < s_quiz_n; i++)
    {
      int guard = 0;
      while (s_quiz_ids[i] == s_quiz_ids[i - 1] && n > 1 && guard < 8)
        {
          s_quiz_ids[i] = (int)(rand() % n);
          guard++;
        }
    }
  show_quiz();
  dm_show(PAGE_WORD_QUIZ);
  if (g_dm_pages[PAGE_WORD_QUIZ])
    {
      dm_face_attach(g_dm_pages[PAGE_WORD_QUIZ], (DM_SCR_W - 64) / 2, 32);
      if (g_dm_face)
        {
          lv_obj_set_size(g_dm_face, 64, 64);
        }
    }
  if (s_say)
    {
      lv_label_set_text(s_say, "");
    }
  if (s_say_q)
    {
      lv_label_set_text(s_say_q, "");
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


int dm_word_today_n(void)
{
  time_t tw = time(NULL);
  struct tm tmw;
  int key;
  localtime_r(&tw, &tmw);
  key = tmw.tm_yday + tmw.tm_year * 1000;
  if (s_ws.today_key != key)
    {
      return 0;
    }
  return s_ws.today_learn;
}

void dm_create_word(void)
{
  lv_obj_t *page;
  lv_obj_t *sc;
  lv_obj_t *card;
  int i;
  char leftb[24];

  srand((unsigned)time(NULL));
  store_load();
  if (s_ws.daily_goal < 5)
    {
      s_ws.daily_goal = 10;
    }
  if (s_ws.daily_goal > DM_ROUND_MAX)
    {
      s_ws.daily_goal = DM_ROUND_MAX;
    }

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
  /* face sits top-center (attached later at 72px) */
  s_say = dm_lbl(page, "", "", g_dm_font_s, C_INK);
  lv_obj_set_style_text_align(s_say, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(s_say, 300);
  lv_obj_align(s_say, LV_ALIGN_TOP_MID, 0, 110);
  card = lv_obj_create(page);
  lv_obj_set_size(card, 292, 70);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 128);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, cb_flip, LV_EVENT_CLICKED, NULL);
  s_w_en = dm_lbl(card, "word", "word", g_dm_font_l, C_INK);
  lv_obj_set_style_text_align(s_w_en, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(s_w_en, 270);
  lv_obj_align(s_w_en, LV_ALIGN_TOP_MID, 0, 12);
  s_w_cn = dm_lbl(card, "tap", "tap", g_dm_font_s, C_DIM);
  lv_obj_set_style_text_align(s_w_cn, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(s_w_cn, 270);
  lv_obj_align(s_w_cn, LV_ALIGN_TOP_MID, 0, 42);
  /* choices glued to bottom */
  {
    lv_obj_t *b1 = dm_btn(page, "不认识", "No", 144, 36, C_HEART, C_EYE,
                          cb_mark, (void *)(uintptr_t)0);
    lv_obj_align(b1, LV_ALIGN_BOTTOM_LEFT, 12, -8);
    lv_obj_t *b2 = dm_btn(page, "认识", "Yes", 144, 36, C_OK, C_EYE, cb_mark,
                          (void *)(uintptr_t)1);
    lv_obj_align(b2, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
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
  /* face top-center 64px; word below; options 2×2 bottom */
  s_q_word = dm_lbl(page, "w", "w", g_dm_font_m, C_INK);
  lv_obj_set_style_text_align(s_q_word, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(s_q_word, 280);
  lv_obj_align(s_q_word, LV_ALIGN_TOP_MID, 0, 100);
  s_q_sub = dm_lbl(page, "pick", "pick", g_dm_font_s, C_DIM);
  lv_obj_set_style_text_align(s_q_sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_q_sub, LV_ALIGN_TOP_MID, 0, 122);
  s_say_q = dm_lbl(page, "", "", g_dm_font_s, C_DIM);
  lv_obj_set_style_text_align(s_say_q, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(s_say_q, 300);
  lv_obj_align(s_say_q, LV_ALIGN_TOP_MID, 0, 84);
  for (i = 0; i < 4; i++)
    {
      int ox = (i % 2) ? 164 : 12;
      int oy = (i < 2) ? 136 : 170;
      s_q_opts[i] = dm_btn(page, "", "", 144, 30, C_BTN_HI, C_INK, cb_qopt,
                           (void *)(uintptr_t)i);
      lv_obj_set_pos(s_q_opts[i], ox, oy);
      lv_obj_set_style_radius(s_q_opts[i], 10, LV_PART_MAIN);
      if (lv_obj_get_child(s_q_opts[i], 0))
        {
          lv_label_set_long_mode(lv_obj_get_child(s_q_opts[i], 0),
                                 LV_LABEL_LONG_DOT);
          lv_obj_set_width(lv_obj_get_child(s_q_opts[i], 0), 128);
        }
    }
  s_q_next = dm_btn(page, "下一题", "Next", 120, 26, C_FACE, C_EYE, cb_qnext,
                    NULL);
  lv_obj_align(s_q_next, LV_ALIGN_BOTTOM_MID, 0, -6);
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

/* ~50 click lines for word pages */
void dm_word_face_click(void)
{
  static const char *zh[] = {
    "这个单词记牢它！", "再看一眼释义", "你可以的，继续背",
    "错的进错题本就好", "今天也在变厉害", "深呼吸，下一个",
    "记不住很正常，多来几次", "我在陪你一起记", "联想一下会更好记",
    "读出声记得更牢哦", "已经学了很多啦", "别急，慢慢来",
    "这个词很常用", "把词根拆开看看", "造个句子试试",
    "想象一下画面", "和昨天学的连起来", "小步快跑，稳",
    "你专注的样子真棒", "记住一个赚一个", "复习比新学更重要",
    "眼睛累就眨眨眼", "把手机放远一点", "今天目标快到了",
    "这个词你会用了吗", "试着英译中再中译英", "睡前再过一遍",
    "早上记性更好哦", "记完给自己点个赞", "错题本在等你翻牌",
    "单词就像朋友，多见面", "一次记不住就两次", "你比想象中记得快",
    "保持这个节奏", "我也在努力眨眼陪你", "要不要先复习三个？",
    "下一个可能更简单", "把发音也记一下", "别和别人比，和昨天比",
    "休息 10 秒再继续", "记忆需要间隔重复", "这一组快结束啦",
    "你已经很棒了", "坚持住，胜利在望", "喝水了吗？",
    "坐姿端正记得更牢", "听我一句：你可以", "把难词标星号",
    "今天学的明天再测", "好耶，又进一步", "爱你哦，继续加油",
  };
  static const char *en[] = {
    "Lock this word in!", "Peek the meaning again", "You got this",
    "Wrong ones go to the book", "Getting better today", "Breathe, next one",
    "Forgetting is normal", "Learning with you", "联想 helps memory",
    "Say it out loud", "You've learned a lot", "No rush",
    "This one is common", "Break the root down", "Try a sentence",
    "Picture it in your head", "Link with yesterday", "Small steps win",
    "You look focused — nice", "One word = one win", "Review > new",
    "Blink if eyes are tired", "Phone farther away", "Daily goal close",
    "Can you use it yet?", "En→Cn then Cn→En", "Skim before sleep",
    "Morning memory is strong", "High-five yourself", "Wrong book awaits",
    "Words are friends — meet more", "Twice if once fails", "Faster than you think",
    "Keep this pace", "Blinking with you", "Review 3 first?",
    "Next might be easier", "Remember the sound", "Beat yesterday, not others",
    "10s break then go", "Spaced repetition wins", "Almost done with set",
    "You're doing great", "Victory is near", "Water break?",
    "Sit straight, recall better", "You can — I mean it", "Star the hard ones",
    "Test today's words tomorrow", "Yay — one step further", "Love you — keep going",
  };
  static int idx;
  int n = (int)(sizeof(zh) / sizeof(zh[0]));
  int decor = idx % 3;

  if (s_say)
    {
      lv_label_set_text(s_say, dm_t(zh[idx], en[idx]));
    }
  if (s_say_q)
    {
      lv_label_set_text(s_say_q, dm_t(zh[idx], en[idx]));
    }
  if (decor == 0)
    {
      dm_face_set(FACE_HAPPY, EYE_DECOR_STAR, 6);
    }
  else if (decor == 1)
    {
      dm_face_set(FACE_LOVE, EYE_DECOR_HEART, 6);
    }
  else
    {
      dm_face_set(FACE_WINK, EYE_DECOR_NONE, 4);
    }
  idx = (idx + 1) % n;
  s_cheer_n++;
}

#endif /* CONFIG_DESKMATE_APP */
