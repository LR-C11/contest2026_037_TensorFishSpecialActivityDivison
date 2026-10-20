/*
 * dm_tts_voice.c — voice profiles + persistence
 */

#include "deskmate.h"
#include "dm_tts_voice.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <string.h>

#define VOICE_CFG "/data/deskmate_voice.txt"

/* Multi-style voices: cloud id + offline pitch */
static const dm_voice_profile_t s_profiles[] = {
  { "冰糖",     "alloy",   780, 0 },  /* 清亮女声 */
  { "茉莉",     "nova",    620, 0 },  /* 温柔女声 */
  { "小云",     "shimmer", 900, 0 },  /* 活泼女声 */
  { "苏打",     "echo",    420, 0 },  /* 阳光男声 */
  { "白桦",     "onyx",    280, 0 },  /* 沉稳男声 */
  { "小川",     "fable",   360, 0 },  /* 青年男声 */
  { "电子音",   "alloy",   1100, 1 }, /* 离线高音提示 */
  { "深海",     "onyx",    180, 1 },  /* 离线低音提示 */
};

#define NPROF ((int)(sizeof(s_profiles) / sizeof(s_profiles[0])))

static int s_sel;

int dm_voice_profile_count(void)
{
  return NPROF;
}

const dm_voice_profile_t *dm_voice_profile_get(int idx)
{
  if (idx < 0 || idx >= NPROF)
    {
      return &s_profiles[0];
    }
  return &s_profiles[idx];
}

int dm_voice_profile_sel(void)
{
  return s_sel;
}

void dm_voice_profile_set(int idx)
{
  FILE *f;
  if (idx < 0)
    {
      idx = 0;
    }
  if (idx >= NPROF)
    {
      idx = NPROF - 1;
    }
  s_sel = idx;
  f = fopen(VOICE_CFG, "w");
  if (f)
    {
      fprintf(f, "%d\n", s_sel);
      fclose(f);
    }
}

void dm_voice_profile_init(void)
{
  FILE *f = fopen(VOICE_CFG, "r");
  int v = 0;
  if (f)
    {
      if (fscanf(f, "%d", &v) == 1)
        {
          if (v >= 0 && v < NPROF)
            {
              s_sel = v;
            }
        }
      fclose(f);
    }
}

const char *dm_voice_profile_name(void)
{
  return s_profiles[s_sel].name;
}

const char *dm_voice_profile_api_id(void)
{
  return s_profiles[s_sel].api_id;
}

int dm_voice_profile_pitch(void)
{
  return s_profiles[s_sel].pitch_hz;
}

#endif /* CONFIG_DESKMATE_APP */
