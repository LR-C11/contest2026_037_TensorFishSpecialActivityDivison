/*
 * dm_proactive.c — proactive notices with OFFLINE fallback
 *
 * Offline: play prebaked WAV under /resource/audio + face/text cue
 * Online:  try cloud TTS for flexible phrases, else same WAV
 */

#include "deskmate.h"
#include "dm_voice.h"
#include "dm_audio_vol.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int s_med_ann_hour = -1;
static int s_water_ann_min = -1;
static int s_brief_ann_day = -1;
static int s_focus_ann = -1;

/* play local wav via same path as TTS output */
static void say_offline(const char *wav_rel, const char *text)
{
  char path[64];
  FILE *f;

  snprintf(path, sizeof(path), "/resource/audio/%s", wav_rel);
  f = fopen(path, "rb");
  if (!f)
    {
      snprintf(path, sizeof(path), "/data/audio/%s", wav_rel);
      f = fopen(path, "rb");
    }
  if (f)
    {
      fclose(f);
      extern int dm_voice_play_file(const char *path);
      if (dm_voice_play_file(path) == 0)
        {
          if (text)
            {
              dm_say(text, text);
            }
          return;
        }
    }

  /* last resort: short beep */
  (void)dm_vol_test_beep();
  if (text)
    {
      dm_say(text, text);
    }
}

static void say(const char *wav, const char *text)
{
  /* prefer offline wav so proactive works without network */
  say_offline(wav, text);
}

static int med_due_count(void)
{
  FILE *f = fopen("/data/deskmate_med.txt", "r");
  char line[160];
  int due = 0;
  time_t now = time(NULL);
  struct tm tmv;
  int hour;

  if (!f)
    {
      return 0;
    }
  localtime_r(&now, &tmv);
  hour = tmv.tm_hour;

  while (fgets(line, sizeof(line), f))
    {
      char name[32];
      int times = 0;
      int h0 = 0, h1 = 0, h2 = 0;
      int taken = 0, day = 0;
      if (sscanf(line, "%31[^|]|%d|%d,%d,%d|%d|%d",
                 name, &times, &h0, &h1, &h2, &taken, &day) == 7)
        {
          int today = (int)(now / 86400);
          int hs[3] = { h0, h1, h2 };
          int k;

          if (day != today)
            {
              taken = 0;
            }
          for (k = 0; k < times && k < 3; k++)
            {
              if (!(taken & (1u << k)) && hour >= hs[k] && hour < hs[k] + 2)
                {
                  due++;
                  break;
                }
            }
        }
    }
  fclose(f);
  return due;
}

static int water_due(void)
{
  FILE *f = fopen("/data/deskmate_water.txt", "r");
  int cups = 0, iv = 30;
  long due = 0;
  int goal = 8;

  if (!f)
    {
      return 0;
    }
  if (fscanf(f, "%d %d %ld %d", &cups, &iv, &due, &goal) < 3)
    {
      fclose(f);
      return 0;
    }
  fclose(f);
  if (cups >= goal)
    {
      return 0;
    }
  return (due != 0 && time(NULL) >= (time_t)due) ? 1 : 0;
}

void dm_proactive_tick(void)
{
  time_t now = time(NULL);
  struct tm tmv;
  int n;

  localtime_r(&now, &tmv);

  if (tmv.tm_hour == 8 && tmv.tm_min >= 30 && tmv.tm_min < 35 &&
      s_brief_ann_day != tmv.tm_yday)
    {
      s_brief_ann_day = tmv.tm_yday;
      say("brief.wav", "早上好。今天也要好好照顾自己");
      return;
    }

  n = med_due_count();
  if (n > 0 && tmv.tm_hour != s_med_ann_hour)
    {
      s_med_ann_hour = tmv.tm_hour;
      say("med.wav", "该吃药了");
      return;
    }

  if (water_due() && tmv.tm_min != s_water_ann_min)
    {
      s_water_ann_min = tmv.tm_min;
      say("water.wav", "喝水时间到");
    }
}

void dm_proactive_focus_done(int32_t minutes)
{
  (void)minutes;
  if (s_focus_ann)
    {
      return;
    }
  s_focus_ann = 1;
  say("focus.wav", "专注完成，休息一下");
  s_focus_ann = 0;
}

#endif /* CONFIG_DESKMATE_APP */
