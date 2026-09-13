/****************************************************************************
 * dm_face.c — geometric face: blink, look, more expressions, no click zoom
 ****************************************************************************/

#include "deskmate.h"

#ifdef CONFIG_DESKMATE_APP

static int s_blink_ph;
static int s_blink_hold;
static int s_blink_gap = 8;
static int s_idle_t;
static dm_face_t s_paint_face = FACE_IDLE;
static int s_home_click_idx;

static void paint(void);

static void mk_part(lv_obj_t *parent, lv_obj_t **out, uint32_t col, int w,
                    int h, int radius)
{
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, radius, LV_PART_MAIN);
  lv_obj_set_style_bg_color(o, lv_color_hex(col), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); /* shown by paint when needed */
  *out = o;
}

void dm_face_build(lv_obj_t *parent, int x, int y, int size)
{
  lv_obj_t *face;

  if (g_dm_face)
    {
      lv_obj_set_parent(g_dm_face, parent);
      lv_obj_set_pos(g_dm_face, x, y);
      lv_obj_set_style_transform_zoom(g_dm_face, LV_SCALE_NONE, LV_PART_MAIN);
      return;
    }

  face = lv_obj_create(parent);
  lv_obj_remove_style_all(face);
  lv_obj_set_size(face, size, size);
  lv_obj_set_pos(face, x, y);
  lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(face, lv_color_hex(C_FACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(face, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(face, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(face, 0, LV_PART_MAIN);
  lv_obj_set_style_clip_corner(face, false, LV_PART_MAIN);
  lv_obj_clear_flag(face, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(face, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(face, dm_face_on_click, LV_EVENT_CLICKED, NULL);
  g_dm_face = face;

  mk_part(face, &g_dm_eye_l, C_EYE, 14, 16, LV_RADIUS_CIRCLE);
  mk_part(face, &g_dm_eye_r, C_EYE, 14, 16, LV_RADIUS_CIRCLE);
  mk_part(face, &g_dm_mouth, C_EYE, 18, 5, 10);
  mk_part(face, &g_dm_decor_l, C_STAR, 8, 8, LV_RADIUS_CIRCLE);
  mk_part(face, &g_dm_decor_r, C_STAR, 8, 8, LV_RADIUS_CIRCLE);
  mk_part(face, &g_dm_blush_l, C_BLUSH, 12, 10, LV_RADIUS_CIRCLE);
  mk_part(face, &g_dm_blush_r, C_BLUSH, 12, 10, LV_RADIUS_CIRCLE);

  lv_obj_set_style_bg_opa(g_dm_eye_l, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_dm_eye_r, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_dm_mouth, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_clear_flag(g_dm_eye_l, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_dm_eye_r, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_dm_mouth, LV_OBJ_FLAG_HIDDEN);

  dm_face_set(FACE_IDLE, EYE_DECOR_NONE, 0);
  s_blink_ph = 0;
  s_blink_hold = 4;
  paint();
}

void dm_face_attach(lv_obj_t *page, int x, int y)
{
  if (!g_dm_face || !page)
    {
      return;
    }
  if (lv_obj_get_parent(g_dm_face) != page)
    {
      lv_obj_set_parent(g_dm_face, page);
    }
  lv_obj_set_pos(g_dm_face, x, y);
  lv_obj_set_style_transform_zoom(g_dm_face, LV_SCALE_NONE, LV_PART_MAIN);
  lv_obj_move_foreground(g_dm_face);
  if (g_dm_eye_l)
    {
      lv_obj_move_foreground(g_dm_eye_l);
    }
  if (g_dm_eye_r)
    {
      lv_obj_move_foreground(g_dm_eye_r);
    }
  if (g_dm_mouth)
    {
      lv_obj_move_foreground(g_dm_mouth);
    }
}

void dm_face_set(dm_face_t f, dm_eye_decor_t decor, int decor_hold)
{
  g_dm.face = f;
  g_dm.decor = decor;
  g_dm.decor_hold = decor_hold;
  if (decor == EYE_DECOR_WIDE)
    {
      g_dm.wide_hold = decor_hold;
    }
  s_paint_face = f;
}

static void paint_mouth(int size)
{
  int mw = 16;
  int mh = 4;
  int my = size * 62 / 100;

  if (!g_dm_mouth)
    {
      return;
    }

  switch (s_paint_face)
    {
      case FACE_HAPPY:
      case FACE_DONE:
        mw = 22;
        mh = 5;
        break;
      case FACE_LAUGH:
        mw = 24;
        mh = 12;
        break;
      case FACE_LOVE:
        mw = 18;
        mh = 4;
        break;
      case FACE_SURPRISE:
        mw = 12;
        mh = 12;
        break;
      case FACE_FOCUS:
      case FACE_THINK:
        mw = 12;
        mh = 3;
        break;
      case FACE_CRY:
        mw = 14;
        mh = 4;
        my = size * 64 / 100;
        break;
      case FACE_ANGRY:
        mw = 14;
        mh = 3;
        break;
      case FACE_SLEEPY:
        mw = 10;
        mh = 3;
        break;
      case FACE_SHY:
        mw = 14;
        mh = 4;
        break;
      case FACE_WINK:
        mw = 18;
        mh = 5;
        break;
      default:
        break;
    }

  lv_obj_set_size(g_dm_mouth, mw, mh);
  lv_obj_set_pos(g_dm_mouth, size / 2 - mw / 2, my);
  lv_obj_set_style_radius(g_dm_mouth, mh <= 4 ? 6 : 10, LV_PART_MAIN);
  lv_obj_clear_flag(g_dm_mouth, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_opa(g_dm_mouth, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_dm_mouth, lv_color_hex(C_EYE), LV_PART_MAIN);
}

static void paint_blush(int size, bool show)
{
  if (!g_dm_blush_l || !g_dm_blush_r)
    {
      return;
    }
  if (!show)
    {
      lv_obj_set_style_bg_opa(g_dm_blush_l, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(g_dm_blush_r, LV_OPA_TRANSP, LV_PART_MAIN);
      return;
    }
  lv_obj_set_style_bg_opa(g_dm_blush_l, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_dm_blush_r, LV_OPA_60, LV_PART_MAIN);
  lv_obj_set_pos(g_dm_blush_l, size * 12 / 100, size * 52 / 100);
  lv_obj_set_pos(g_dm_blush_r, size * 78 / 100, size * 52 / 100);
}

static void paint_decor(int size)
{
  if (!g_dm_decor_l || !g_dm_decor_r)
    {
      return;
    }

  if (g_dm.decor == EYE_DECOR_NONE || s_blink_ph == 2)
    {
      lv_obj_set_style_bg_opa(g_dm_decor_l, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(g_dm_decor_r, LV_OPA_TRANSP, LV_PART_MAIN);
      return;
    }

  uint32_t col = C_STAR;
  int d = 8;
  if (g_dm.decor == EYE_DECOR_HEART)
    {
      col = C_HEART;
      d = 9;
    }
  else if (g_dm.decor == EYE_DECOR_WIDE)
    {
      col = C_ACCENT;
      d = 6;
    }

  lv_obj_set_style_bg_color(g_dm_decor_l, lv_color_hex(col), LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_dm_decor_r, lv_color_hex(col), LV_PART_MAIN);
  lv_obj_set_size(g_dm_decor_l, d, d);
  lv_obj_set_size(g_dm_decor_r, d, d);
  lv_obj_set_style_bg_opa(g_dm_decor_l, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_dm_decor_r, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_pos(g_dm_decor_l, size * 18 / 100, size * 18 / 100);
  lv_obj_set_pos(g_dm_decor_r, size * 74 / 100, size * 18 / 100);
}

static void paint(void)
{
  int size = 80;
  bool wide = false;
  bool wink_l = false;
  bool wink_r = false;
  bool sleepy = false;
  bool blush = false;
  int base_w;
  int base_h;
  int ew;
  int eh;
  int lx = g_dm.look_x;
  int ly = g_dm.look_y;
  int ey = size * 30 / 100;

  if (!g_dm_face)
    {
      return;
    }

  size = lv_obj_get_width(g_dm_face);

  switch (s_paint_face)
    {
      case FACE_SURPRISE:
        wide = true;
        break;
      case FACE_WINK:
        wink_r = true;
        break;
      case FACE_SLEEPY:
        sleepy = true;
        break;
      case FACE_SHY:
      case FACE_LOVE:
        blush = true;
        break;
      case FACE_FOCUS:
      case FACE_THINK:
        /* slightly narrowed */
        break;
      default:
        break;
    }

  if (g_dm.decor == EYE_DECOR_WIDE || g_dm.wide_hold > 0)
    {
      wide = true;
    }

  base_w = size > 70 ? 12 : 10;
  base_h = size > 70 ? 14 : 12;
  ew = wide ? base_w + 6 : base_w;
  eh = wide ? base_h + 8 : base_h;
  if (s_paint_face == FACE_FOCUS || s_paint_face == FACE_THINK)
    {
      eh = base_h - 2;
    }
  if (sleepy)
    {
      eh = 6;
      ew = base_w + 6;
      ly = 2;
    }

  if (s_blink_ph == 2 || sleepy)
    {
      eh = sleepy ? 5 : 3;
      ew = base_w + 10;
      lx = 0;
      ly = sleepy ? 2 : 0;
    }
  else if (s_blink_ph == 1)
    {
      eh = 7;
    }

  lv_obj_set_size(g_dm_eye_l, ew, eh);
  lv_obj_set_size(g_dm_eye_r, ew, eh);
  lv_obj_set_pos(g_dm_eye_l, size / 2 - size / 4 - ew / 2 + lx, ey + ly);
  lv_obj_set_pos(g_dm_eye_r, size / 2 + size / 4 - ew / 2 - lx, ey + ly);
  lv_obj_clear_flag(g_dm_eye_l, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_dm_eye_r, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_opa(g_dm_eye_l, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(g_dm_eye_r, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_dm_eye_l, lv_color_hex(C_EYE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_dm_eye_r, lv_color_hex(C_EYE), LV_PART_MAIN);

  if (wink_l)
    {
      lv_obj_set_size(g_dm_eye_l, ew + 8, 3);
    }
  if (wink_r)
    {
      lv_obj_set_size(g_dm_eye_r, ew + 8, 3);
    }

  /* tear under one eye for cry */
  if (s_paint_face == FACE_CRY && g_dm_decor_r)
    {
      lv_obj_set_style_bg_color(g_dm_decor_r, lv_color_hex(C_TEAR),
                                LV_PART_MAIN);
      lv_obj_set_size(g_dm_decor_r, 5, 8);
      lv_obj_set_pos(g_dm_decor_r, size * 68 / 100, ey + eh + 2);
      lv_obj_set_style_bg_opa(g_dm_decor_r, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(g_dm_decor_l, LV_OPA_TRANSP, LV_PART_MAIN);
    }
  else
    {
      paint_decor(size);
    }

  paint_blush(size, blush);
  paint_mouth(size);
}

void dm_face_tick(void)
{
  if (!g_dm_face)
    {
      return;
    }

  s_idle_t++;

  /* sleepy blinks slower */
  int gap_add = (s_paint_face == FACE_SLEEPY) ? 8 : 0;

  if (s_blink_hold > 0)
    {
      s_blink_hold--;
    }
  else if (s_blink_ph == 0)
    {
      if (s_blink_gap > 0)
        {
          s_blink_gap--;
        }
      else
        {
          s_blink_ph = 1;
          s_blink_hold = 1;
        }
    }
  else if (s_blink_ph == 1)
    {
      s_blink_ph = 2;
      s_blink_hold = 1;
    }
  else
    {
      s_blink_ph = 0;
      s_blink_hold = 0;
      s_blink_gap = 6 + gap_add + (s_idle_t % 10);
    }

  if (g_dm.decor_hold > 0)
    {
      g_dm.decor_hold--;
      if (g_dm.decor_hold == 0 && g_dm.page == PAGE_FOCUS_RUN &&
          g_dm.focus_run)
        {
          g_dm.decor = EYE_DECOR_NONE;
          dm_face_set(FACE_FOCUS, EYE_DECOR_NONE, 0);
        }
    }

  if (g_dm.wide_hold > 0)
    {
      g_dm.wide_hold--;
    }

  if (s_idle_t % 6 == 0)
    {
      g_dm.look_x = ((s_idle_t / 6) % 5) - 2;
      g_dm.look_y = ((s_idle_t / 9) % 3) - 1;
    }

  if (g_dm.page == PAGE_FOCUS_RUN && g_dm.focus_run && !g_dm.focus_finished &&
      g_dm.decor == EYE_DECOR_NONE && s_idle_t % 28 == 0)
    {
      typedef struct
      {
        dm_face_t face;
        dm_eye_decor_t decor;
        int hold;
        const char *zh;
        const char *en;
      } ambient_t;

      static const ambient_t amb[] = {
        { FACE_FOCUS, EYE_DECOR_STAR, 8, "星星眼，状态不错",
          "Sparkle eyes — nice pace" },
        { FACE_LOVE, EYE_DECOR_HEART, 8, "给你比个心，继续",
          "Heart eyes — keep going" },
        { FACE_SURPRISE, EYE_DECOR_WIDE, 6, "睁大眼睛看着你",
          "Watching you — stay with it" },
        { FACE_WINK, EYE_DECOR_NONE, 6, "眨个眼，你可以的",
          "Wink — you've got this" },
        { FACE_LAUGH, EYE_DECOR_NONE, 6, "嘿嘿，状态来了",
          "Heh — you're in the zone" },
        { FACE_THINK, EYE_DECOR_NONE, 6, "我在想你还能更专注",
          "Thinking you can go deeper" },
        { FACE_SHY, EYE_DECOR_NONE, 6, "被你专注的样子帅到了",
          "A bit shy — you look cool focusing" },
        { FACE_HAPPY, EYE_DECOR_NONE, 6, "节奏很稳，保持住",
          "Solid rhythm — keep it" },
        { FACE_SLEEPY, EYE_DECOR_NONE, 6, "我都有点困了，你还这么拼",
          "I'm sleepy, you're still going" },
        { FACE_FOCUS, EYE_DECOR_STAR, 8, "这一小会儿已经很棒了",
          "This stretch is already great" },
      };

      const ambient_t *a = &amb[(s_idle_t / 28) % (int)(sizeof(amb) /
                                                         sizeof(amb[0]))];
      dm_face_set(a->face, a->decor, a->hold);
      dm_say(a->zh, a->en);
    }

  paint();
}

static const char *s_run_zh[] = {
  "我在呢，专注就好",
  "别走神，我帮你看着时间",
  "深呼吸，再坚持一下",
  "点我也没用，倒计时不会停哦",
  "你认真起来还挺好看的",
  "要不要先喝口水？",
  "这一段完成就休息",
  "手机先放远一点嘛",
  "肩膀放松一点",
  "眼睛累了可以看看远处",
  "我在帮你数秒呢",
  "就差这一会儿了",
  "做得好，继续保持",
  "分心了也没关系，回来就好",
  "要不要把目标再缩小一点？",
};
static const char *s_run_en[] = {
  "I'm here. Just focus.",
  "Eyes on the task — I'll watch the clock.",
  "Breathe. One more push.",
  "Tapping me won't pause time.",
  "You look focused. Nice.",
  "Maybe sip some water?",
  "Rest after this block.",
  "Phone a little farther away?",
  "Loosen your shoulders.",
  "Look far away if eyes tire.",
  "I'm counting the seconds for you.",
  "Almost there.",
  "Nice — keep this pace.",
  "It's okay. Come back when ready.",
  "Want a smaller goal for now?",
};

static const char *s_home_zh[] = {
  "嗨，我在呢",
  "今天想专注多久呀？",
  "点下面时长，再点开始",
  "摸摸头，准备好了吗？",
  "我会帮你看着时间的",
  "无聊也可以先找我聊天",
  "25 分钟挺合适的，试试？",
  "自选时长也行哦",
  "开始之后我就不吵你啦",
  "先选个预设呗",
  "想监督一件事也可以叫我",
  "备忘忘了什么？随时说",
};
static const char *s_home_en[] = {
  "Hi, I'm here",
  "How long do you want to focus?",
  "Pick a duration, then Start",
  "Pat pat — ready?",
  "I'll watch the clock for you",
  "You can chat with me when bored",
  "25 minutes is a sweet spot?",
  "Custom duration works too",
  "Once you start, I'll stay quiet-ish",
  "Try a preset first",
  "Need a task supervisor? I'm in",
  "Forgot something? Tell me to note it",
};

static const dm_face_t s_home_faces[] = {
  FACE_HAPPY, FACE_IDLE,     FACE_WINK,  FACE_SHY,  FACE_LAUGH, FACE_LOVE,
  FACE_FOCUS, FACE_SURPRISE, FACE_THINK, FACE_HAPPY, FACE_WINK, FACE_IDLE,
};

#define DM_ARR_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

static int s_run_click_idx;

void dm_face_on_click(lv_event_t *e)
{
  (void)e;

  if (!g_dm_face)
    {
      return;
    }

  /* no zoom — keep size stable */
  if (g_dm.page == PAGE_FOCUS_RUN)
    {
      int n = DM_ARR_LEN(s_run_zh);
      dm_say(s_run_zh[s_run_click_idx], s_run_en[s_run_click_idx]);
      s_run_click_idx = (s_run_click_idx + 1) % n;
      if (s_run_click_idx % 4 == 0)
        {
          dm_face_set(FACE_HAPPY, EYE_DECOR_NONE, 4);
        }
      else if (s_run_click_idx % 4 == 1)
        {
          dm_face_set(FACE_WINK, EYE_DECOR_NONE, 4);
        }
      else if (s_run_click_idx % 4 == 2)
        {
          dm_face_set(FACE_LAUGH, EYE_DECOR_NONE, 4);
        }
      else
        {
          dm_face_set(FACE_LOVE, EYE_DECOR_NONE, 4);
        }
    }
  else
    {
      int n = DM_ARR_LEN(s_home_zh);
      dm_say(s_home_zh[s_home_click_idx], s_home_en[s_home_click_idx]);
      dm_face_set(s_home_faces[s_home_click_idx], EYE_DECOR_NONE, 5);
      s_home_click_idx = (s_home_click_idx + 1) % n;
    }
}

#endif /* CONFIG_DESKMATE_APP */
