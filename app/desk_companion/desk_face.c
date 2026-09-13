/****************************************************************************
 * desk_face.c — geometric face (no fancy unicode) + expression engine
 ****************************************************************************/

#include "desk_companion.h"

#ifdef CONFIG_DESK_COMPANION_APP

typedef enum { BLINK_OPEN = 0, BLINK_HALF, BLINK_SHUT } blink_ph_t;
static blink_ph_t g_bp;
static int g_blink_hold;
static int g_blink_gap;
static int g_face_hold;
static int g_idle_t;
static int g_anim_t;
static int g_look_x;
static int g_look_y;
static int g_zzz_phase;
static int g_chat_step;

/* mouth drawn as rounded bar / circle — no font glyphs */
typedef enum {
  MOUTH_SMILE = 0,
  MOUTH_FLAT,
  MOUTH_O,
  MOUTH_FROWN,
  MOUTH_OPEN,
} mouth_t;

void desk_set_face_hold(desk_face_t f, int32_t ticks)
{
  g_face = f;
  g_face_hold = (int)ticks;
  g_idle_t = 0;
}

void desk_set_face(desk_face_t f)
{
  desk_set_face_hold(f, 20);
}

static void make_eye(lv_obj_t *parent, lv_obj_t **out)
{
  lv_obj_t *e = lv_obj_create(parent);
  lv_obj_set_size(e, 14, 14);
  lv_obj_set_style_radius(e, 100, LV_PART_MAIN);
  lv_obj_set_style_bg_color(e, lv_color_hex(C_EYE), LV_PART_MAIN);
  lv_obj_set_style_border_width(e, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(e, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(e, LV_OBJ_FLAG_SCROLLABLE);
  *out = e;
}

static void make_soft(lv_obj_t *parent, lv_obj_t **out, uint32_t col, int w,
                      int h)
{
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, 100, LV_PART_MAIN);
  lv_obj_set_style_bg_color(o, lv_color_hex(col), LV_PART_MAIN);
  lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, 0, LV_PART_MAIN);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  *out = o;
}

static void paint_eyes(int ew, int eh, int lx, int ly, bool hide_l, bool hide_r)
{
  int w;
  int h;

  if (!g_eye_l || !g_eye_r)
    return;
  lv_obj_set_style_bg_opa(g_eye_l, hide_l ? LV_OPA_TRANSP : LV_OPA_COVER,
                          LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_eye_r, hide_r ? LV_OPA_TRANSP : LV_OPA_COVER,
                          LV_PART_MAIN);
  w = ew;
  h = eh;
  if (eh <= 4)
    w = ew + 10;
  lv_obj_set_size(g_eye_l, w, h);
  lv_obj_set_size(g_eye_r, w, h);
  lv_obj_set_pos(g_eye_l, 22 + lx, 26 + ly);
  lv_obj_set_pos(g_eye_r, 52 - lx, 26 + ly);
  lv_obj_set_style_radius(g_eye_l, h <= 6 ? 6 : 100, LV_PART_MAIN);
  lv_obj_set_style_radius(g_eye_r, h <= 6 ? 6 : 100, LV_PART_MAIN);
}

static void paint_mouth(mouth_t m, int open_amt)
{
  int w;
  int h;

  if (!g_mouth)
    return;
  /* g_mouth is a black rounded object, not a label */
  switch (m)
    {
      case MOUTH_SMILE:
        w = 22;
        h = 5;
        break;
      case MOUTH_FLAT:
        w = 16;
        h = 3;
        break;
      case MOUTH_O:
        w = (open_amt > 8) ? 14 : 10;
        h = w;
        break;
      case MOUTH_FROWN:
        w = 18;
        h = 5;
        break;
      case MOUTH_OPEN:
        w = 16;
        h = 10 + (open_amt % 6);
        break;
      default:
        w = 18;
        h = 4;
        break;
    }
  lv_obj_set_size(g_mouth, w, h);
  lv_obj_set_style_radius(g_mouth, (m == MOUTH_O || m == MOUTH_OPEN) ? 100 : 8,
                          LV_PART_MAIN);
  lv_obj_set_pos(g_mouth, 44 - w / 2, 52);
  lv_obj_set_style_bg_color(g_mouth, lv_color_hex(C_EYE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_mouth, LV_OPA_COVER, LV_PART_MAIN);
}

void desk_apply_face(void)
{
  int m = (g_bp == BLINK_HALF) ? 7 : (g_bp == BLINK_SHUT ? 3 : 14);
  int ew = 14;
  int eh = m;
  int lx = g_look_x;
  int ly = g_look_y;
  bool hide_l = false;
  bool hide_r = false;
  mouth_t mouth = MOUTH_SMILE;
  int open_amt = 0;
  uint32_t face_col = C_FACE;
  uint32_t blush = 0;
  int zzz_on = 0;
  int think_on = 0;

  switch (g_face)
    {
      case FACE_HAPPY:
        eh = 6;
        ew = 18;
        mouth = MOUTH_SMILE;
        blush = 90;
        break;
      case FACE_FOCUS:
        eh = 5;
        ew = 16;
        mouth = MOUTH_FLAT;
        ly = 2;
        break;
      case FACE_THINK:
        eh = 12;
        ew = 12;
        mouth = MOUTH_O;
        open_amt = 6;
        ly = -4;
        lx = (g_anim_t / 8) % 2 ? 3 : -3;
        think_on = 1;
        break;
      case FACE_SLEEP:
        eh = 3;
        ew = 18;
        mouth = MOUTH_FLAT;
        ly = 2;
        zzz_on = 1;
        break;
      case FACE_WINK:
        eh = 14;
        ew = 14;
        hide_r = true;
        mouth = MOUTH_SMILE;
        blush = 70;
        break;
      case FACE_SURPRISE:
        eh = 16;
        ew = 15;
        mouth = MOUTH_O;
        open_amt = 12;
        break;
      case FACE_LOVE:
        eh = 13;
        ew = 14;
        mouth = MOUTH_SMILE;
        blush = 110;
        break;
      case FACE_SHY:
        eh = 8;
        ew = 14;
        ly = 4;
        mouth = MOUTH_SMILE;
        blush = 120;
        break;
      case FACE_TALK:
        eh = 14;
        ew = 14;
        mouth = MOUTH_OPEN;
        open_amt = g_anim_t % 8;
        break;
      case FACE_SAD:
        eh = 10;
        ew = 13;
        ly = 3;
        mouth = MOUTH_FROWN;
        break;
      case FACE_ANGRY:
        eh = 6;
        ew = 16;
        ly = -2;
        mouth = MOUTH_FLAT;
        break;
      case FACE_CONFUSED:
        eh = 12;
        ew = 13;
        ly = (g_anim_t / 10) % 2 ? -2 : 2;
        mouth = MOUTH_FROWN;
        break;
      case FACE_YAWN:
        eh = 5;
        ew = 14;
        mouth = MOUTH_O;
        open_amt = 10;
        zzz_on = (g_anim_t / 6) % 2;
        break;
      case FACE_NERVOUS:
        eh = (g_anim_t % 3 == 0) ? 8 : 14;
        ew = 12;
        mouth = MOUTH_FLAT;
        blush = 50;
        break;
      case FACE_IDLE:
      default:
        if (g_bp == BLINK_SHUT)
          {
            eh = 3;
            ew = 18;
          }
        else if (g_bp == BLINK_HALF)
          {
            eh = 7;
            ew = 16;
          }
        else
          {
            eh = 14;
            ew = 14;
          }
        mouth = MOUTH_SMILE;
        break;
    }

  paint_eyes(ew, eh, lx, ly, hide_l, hide_r);
  paint_mouth(mouth, open_amt);
  if (g_face_box)
    lv_obj_set_style_bg_color(g_face_box, lv_color_hex(face_col), LV_PART_MAIN);
  if (g_blush_l && g_blush_r)
    {
      lv_obj_set_style_bg_opa(g_blush_l, blush, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(g_blush_r, blush, LV_PART_MAIN);
    }
  if (g_zzz)
    {
      if (zzz_on)
        {
          lv_obj_clear_flag(g_zzz, LV_OBJ_FLAG_HIDDEN);
          /* only ASCII Z/z — always in Montserrat */
          lv_label_set_text(g_zzz, ((g_zzz_phase / 5) % 3) == 1 ? "Z" : "z");
          lv_obj_set_style_opa(g_zzz, 140 + ((g_zzz_phase * 13) % 90),
                               LV_PART_MAIN);
        }
      else
        lv_obj_add_flag(g_zzz, LV_OBJ_FLAG_HIDDEN);
    }
  for (int i = 0; i < 3; i++)
    {
      if (!g_think_dot[i])
        continue;
      if (think_on)
        {
          lv_obj_clear_flag(g_think_dot[i], LV_OBJ_FLAG_HIDDEN);
          lv_obj_set_style_opa(g_think_dot[i],
                               80 + ((g_anim_t * 17 + i * 40) % 140),
                               LV_PART_MAIN);
        }
      else
        lv_obj_add_flag(g_think_dot[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void desk_create_face_widgets(lv_obj_t *face)
{
  g_face_box = face;
  make_eye(face, &g_eye_l);
  make_eye(face, &g_eye_r);
  make_soft(face, &g_blush_l, 0xFFB4A2, 16, 10);
  make_soft(face, &g_blush_r, 0xFFB4A2, 16, 10);
  lv_obj_set_pos(g_blush_l, 8, 40);
  lv_obj_set_pos(g_blush_r, 64, 40);

  /* mouth: geometric object, not a font glyph */
  g_mouth = lv_obj_create(face);
  lv_obj_set_size(g_mouth, 22, 5);
  lv_obj_set_style_radius(g_mouth, 8, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_mouth, lv_color_hex(C_EYE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_mouth, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_mouth, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_mouth, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(g_mouth, 33, 52);

  g_zzz = lv_label_create(face);
  lv_label_set_text(g_zzz, "z");
  style_f(g_zzz, g_font_s, C_MUTED);
  lv_obj_set_pos(g_zzz, 100, 4);
  lv_obj_add_flag(g_zzz, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < 3; i++)
    {
      g_think_dot[i] = lv_obj_create(face);
      int sz = 3 + i;
      lv_obj_set_size(g_think_dot[i], sz, sz);
      lv_obj_set_pos(g_think_dot[i], 108 + i * 5, 22 - i * 7);
      lv_obj_set_style_radius(g_think_dot[i], 100, LV_PART_MAIN);
      lv_obj_set_style_bg_color(g_think_dot[i], lv_color_hex(C_EYE),
                                LV_PART_MAIN);
      lv_obj_set_style_border_width(g_think_dot[i], 0, LV_PART_MAIN);
      lv_obj_add_flag(g_think_dot[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_think_dot[i], LV_OBJ_FLAG_SCROLLABLE);
    }
  paint_eyes(14, 14, 0, 0, false, false);
  paint_mouth(MOUTH_SMILE, 0);
}

void desk_face_on_click(void)
{
  static const desk_face_t faces[] = {
      FACE_HAPPY, FACE_WINK, FACE_LOVE, FACE_SURPRISE, FACE_THINK,
      FACE_CONFUSED, FACE_SHY, FACE_YAWN, FACE_NERVOUS, FACE_TALK};
  static const char *zh[] = {
      "嗨，我在呢", "眨眼~ 被发现啦", "今天也喜欢你", "哇！", "让我想想…",
      "咦？不太明白", "有点害羞", "有点困…", "有点紧张", "再说一遍嘛"};
  static const char *en[] = {
      "Hi, I'm here.", "Wink~", "I like you today.", "Wow!", "Hmm...",
      "Wait, what?", "A bit shy...", "Sleepy...", "A bit nervous...",
      "Say that again?"};
  int n = (int)(sizeof(faces) / sizeof(faces[0]));
  say(zh[g_chat_step % n], en[g_chat_step % n]);
  desk_set_face_hold(faces[g_chat_step % n], 25);
  g_chat_step = (g_chat_step + 1) % n;
}

void desk_face_anim_tick(void)
{
  int prev_bp = g_bp;
  int prev_face = g_face;

  g_anim_t++;
  g_idle_t++;
  g_zzz_phase++;

  if (g_face_hold > 0)
    {
      g_face_hold--;
      if (g_face_hold == 0 && g_face != FACE_SLEEP && g_face != FACE_FOCUS)
        g_face = FACE_IDLE;
    }

  if (g_face != FACE_SLEEP && g_face != FACE_FOCUS && g_idle_t > 90)
    desk_set_face_hold(FACE_SLEEP, 0);

  if (g_face == FACE_IDLE && (g_anim_t % 25) == 0)
    {
      g_look_x = (g_anim_t / 25) % 3 - 1;
      g_look_y = ((g_anim_t / 25) % 2) ? -1 : 0;
    }

  if (g_face == FACE_IDLE || g_face == FACE_HAPPY || g_face == FACE_SHY ||
      g_face == FACE_WINK || g_face == FACE_NERVOUS)
    {
      if (g_blink_hold > 0)
        {
          g_blink_hold--;
          if (g_blink_hold == 0)
            {
              if (g_bp == BLINK_OPEN)
                {
                  g_bp = BLINK_HALF;
                  g_blink_hold = 1;
                }
              else if (g_bp == BLINK_HALF)
                {
                  g_bp = BLINK_SHUT;
                  g_blink_hold = 1;
                }
              else
                {
                  g_bp = BLINK_OPEN;
                  g_blink_gap = 40 + (g_anim_t * 17) % 80;
                }
            }
        }
      else
        {
          g_blink_gap--;
          if (g_blink_gap <= 0)
            {
              g_bp = BLINK_HALF;
              g_blink_hold = 1;
            }
        }
    }
  else
    {
      g_bp = BLINK_OPEN;
      g_blink_hold = 0;
      g_blink_gap = 40;
    }

  if (g_face != prev_face || g_bp != prev_bp || g_look_x != -99 ||
      (g_face == FACE_THINK && (g_anim_t % 8) == 0) ||
      (g_face == FACE_SLEEP && (g_anim_t % 6) == 0) ||
      (g_face == FACE_TALK && (g_anim_t % 4) == 0) ||
      (g_face == FACE_NERVOUS && (g_anim_t % 3) == 0) ||
      (g_face == FACE_CONFUSED && (g_anim_t % 10) == 0))
    {
      desk_apply_face();
      g_look_x = g_look_x; /* keep */
    }
}

#endif
