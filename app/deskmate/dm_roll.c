/****************************************************************************
 * dm_roll.c — MM:SS rolling digit clock (wheel-style digit change)
 ****************************************************************************/

#include "deskmate.h"

#ifdef CONFIG_DESKMATE_APP

typedef struct {
  lv_obj_t *old;
} roll_old_t;

static void set_digit_label(lv_obj_t *lbl, int digit)
{
  char b[2] = { (char)('0' + digit), 0 };
  lv_label_set_text(lbl, b);
}

static void clip_setup(dm_digit_t *d, lv_obj_t *parent, int x, int y, int w,
                       int h, const lv_font_t *font)
{
  d->clip = lv_obj_create(parent);
  lv_obj_set_size(d->clip, w, h);
  lv_obj_set_pos(d->clip, x, y);
  lv_obj_set_style_bg_opa(d->clip, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(d->clip, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(d->clip, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(d->clip, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(d->clip, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(d->clip, 4, LV_PART_MAIN);
  lv_obj_clear_flag(d->clip, LV_OBJ_FLAG_SCROLLABLE);

  d->cur = lv_label_create(d->clip);
  dm_style(d->cur, font, C_INK);
  lv_obj_set_width(d->cur, w);
  lv_obj_set_style_text_align(d->cur, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_pos(d->cur, 0, 0);
  d->digit = -1;
}

static void anim_ready_free_old(lv_anim_t *a)
{
  roll_old_t *p = (roll_old_t *)a->user_data;
  if (!p)
    {
      return;
    }
  if (p->old && lv_obj_is_valid(p->old))
    {
      lv_obj_delete(p->old);
    }
  lv_free(p);
}

static void roll_digit(dm_digit_t *d, int digit, int w, int h,
                       const lv_font_t *font, bool animate)
{
  lv_anim_t a;
  roll_old_t *ud;
  lv_obj_t *nw;

  if (!d->clip)
    {
      return;
    }

  if (digit == d->digit && d->cur)
    {
      return;
    }

  /* kill in-flight roll on this digit so we never stack labels */
  lv_anim_delete(d->cur, NULL);

  nw = lv_label_create(d->clip);
  dm_style(nw, font, C_INK);
  set_digit_label(nw, digit);
  lv_obj_set_width(nw, w);
  lv_obj_set_style_text_align(nw, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

  if (!animate || !d->cur || d->digit < 0)
    {
      if (d->cur && lv_obj_is_valid(d->cur))
        {
          lv_obj_delete(d->cur);
        }
      lv_obj_set_pos(nw, 0, 0);
      d->cur = nw;
      d->digit = digit;
      return;
    }

  ud = lv_malloc(sizeof(roll_old_t));
  if (!ud)
    {
      lv_obj_delete(d->cur);
      lv_obj_set_pos(nw, 0, 0);
      d->cur = nw;
      d->digit = digit;
      return;
    }

  ud->old = d->cur;
  d->cur = nw;
  d->digit = digit;

  lv_obj_set_pos(nw, 0, h);
  lv_anim_init(&a);
  lv_anim_set_var(&a, nw);
  lv_anim_set_values(&a, h, 0);
  lv_anim_set_time(&a, 170);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);

  lv_anim_init(&a);
  lv_anim_set_var(&a, ud->old);
  lv_anim_set_values(&a, 0, -h);
  lv_anim_set_time(&a, 170);
  lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
  lv_anim_set_user_data(&a, ud);
  lv_anim_set_ready_cb(&a, anim_ready_free_old);
  lv_anim_start(&a);
}

void dm_clock_create(dm_clock_t *c, lv_obj_t *parent, int x, int y, int w,
                     int h, const lv_font_t *font)
{
  /* layout: [M][M][:][S][S]  — 4 digit windows + 1 colon */
  int dw = (w * 2) / 10; /* digit width */
  int gap_colon = w - 4 * dw;
  int colon_x;
  int i;

  c->box = lv_obj_create(parent);
  lv_obj_set_size(c->box, w, h);
  lv_obj_set_pos(c->box, x, y);
  lv_obj_set_style_bg_opa(c->box, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(c->box, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c->box, 0, LV_PART_MAIN);
  lv_obj_clear_flag(c->box, LV_OBJ_FLAG_SCROLLABLE);

  for (i = 0; i < 4; i++)
    {
      int dx = (i < 2) ? (i * dw) : (i * dw + gap_colon);
      clip_setup(&c->d[i], c->box, dx, 0, dw, h, font);
    }

  colon_x = 2 * dw + gap_colon / 4;
  {
    lv_obj_t *col = lv_label_create(c->box);
    lv_label_set_text(col, ":");
    dm_style(col, font, C_DIM);
    lv_obj_set_pos(col, colon_x, 0);
  }
}

void dm_clock_set(dm_clock_t *c, int32_t seconds_left)
{
  int32_t left = seconds_left < 0 ? 0 : seconds_left;
  int mm = (int)(left / 60);
  int ss = (int)(left % 60);
  int digits[4];
  int dw;
  int h;
  int i;
  bool animate = true;

  if (!c || !c->box)
    {
      return;
    }

  if (mm > 99)
    {
      mm = 99;
    }

  digits[0] = mm / 10;
  digits[1] = mm % 10;
  digits[2] = ss / 10;
  digits[3] = ss % 10;

  dw = (lv_obj_get_width(c->box) * 2) / 10;
  h = lv_obj_get_height(c->box);

  /* first paint: no roll animation */
  if (c->d[0].digit < 0)
    {
      animate = false;
    }

  for (i = 0; i < 4; i++)
    {
      roll_digit(&c->d[i], digits[i], dw, h, g_dm_font_xl, animate);
    }
}

void dm_clock_refresh_now(dm_clock_t *c)
{
  dm_clock_set(c, g_dm.focus_left);
}

#endif /* CONFIG_DESKMATE_APP */
