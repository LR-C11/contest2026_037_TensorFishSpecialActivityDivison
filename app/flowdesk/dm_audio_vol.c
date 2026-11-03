/*
 * dm_audio_vol.c — volume control for 4R 3W speaker
 * Uses NuttX audio FEATURE/VOLUME ioctl (see nxplayer_setvolume),
 * value range 0..1000 on device, 0..100% in UI.
 * Persist to /data/deskmate_vol.txt
 */

#include "deskmate.h"
#include "dm_audio_vol.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <nuttx/audio/audio.h>
#include <aw-alsa-lib/control.h>
#include "dm_voice.h"

#define VOL_PATH "/data/deskmate_vol.txt"
#define VOL_DEFAULT 80
#define CODEC_CARD "audiocodec"
#define DAC_VOL_MAX 160
#define DAC_ALARM_VOL_MAX 200
#define VOL_ALARM_DEFAULT 100

static pthread_mutex_t s_vol_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_vol = VOL_DEFAULT;
static int s_mute;
static int s_alarm_vol = VOL_ALARM_DEFAULT;
static int s_alarm_active;

/* Perceptual slider mapping: lift the quiet half of the range, while
 * reserving headroom at the top to avoid the previous full-scale loudness. */
static unsigned int vol_percent_to_reg(int percent)
{
  static const struct
  {
    unsigned char percent;
    unsigned char reg;
  } points[] =
  {
    { 0,   0 },
    { 10, 48 },
    { 25, 88 },
    { 50, 124 },
    { 75, 148 },
    { 100, DAC_VOL_MAX }
  };
  unsigned int i;

  if (percent <= 0)
    {
      return 0;
    }
  if (percent >= 100)
    {
      return DAC_VOL_MAX;
    }

  for (i = 1; i < sizeof(points) / sizeof(points[0]); i++)
    {
      if (percent <= points[i].percent)
        {
          unsigned int span = points[i].percent - points[i - 1].percent;
          unsigned int value_span = points[i].reg - points[i - 1].reg;
          unsigned int offset = (unsigned int)percent - points[i - 1].percent;
          return points[i - 1].reg + (value_span * offset + span / 2) / span;
        }
    }

  return DAC_VOL_MAX;
}

static unsigned int alarm_percent_to_reg(int percent)
{
  unsigned int media_reg = vol_percent_to_reg(percent);

  /* The codec control accepts 0..255. Keep media at its existing ceiling,
   * but give alarms a modest app-layer boost while retaining headroom. */
  return (media_reg * DAC_ALARM_VOL_MAX + DAC_VOL_MAX / 2) / DAC_VOL_MAX;
}

static void vol_save_locked(void)
{
  FILE *f = fopen(VOL_PATH, "w");
  if (!f)
    {
      return;
    }
  fprintf(f, "%d %d %d\n", s_vol, s_mute, s_alarm_vol);
  fclose(f);
}

static void vol_apply_locked(void)
{
  unsigned int reg;
  int percent;
  int r1;
  int r2;

  /* Alarm output temporarily overrides both the media gain and media mute;
   * the saved media controls are restored as soon as the spoken alert ends. */
  if (s_alarm_active > 0)
    {
      percent = s_alarm_vol;
      reg = alarm_percent_to_reg(percent);
    }
  else
    {
      percent = s_mute ? 0 : s_vol;
      reg = vol_percent_to_reg(percent);
    }
  r1 = snd_ctl_set(CODEC_CARD, "DACL digital volume", reg);
  r2 = snd_ctl_set(CODEC_CARD, "DACR digital volume", reg);
  syslog(LOG_INFO, "[dm-vol] dac vol reg=%u alarm=%d (dACL=%d dACR=%d)\n",
         reg, s_alarm_active > 0, r1, r2);
  dm_voice_set_speaker_volume(percent);
}

void dm_vol_init(void)
{
  FILE *f = fopen(VOL_PATH, "r");
  int v = VOL_DEFAULT;
  int m = 0;
  int alarm = VOL_ALARM_DEFAULT;

  if (f)
    {
      int fields = fscanf(f, "%d %d %d", &v, &m, &alarm);
      if (fields >= 1)
        {
          if (v < 0) v = 0;
          if (v > 100) v = 100;
          if (alarm < 0) alarm = 0;
          if (alarm > 100) alarm = 100;
          pthread_mutex_lock(&s_vol_lock);
          s_vol = v;
          s_mute = m ? 1 : 0;
          s_alarm_vol = fields >= 3 ? alarm : VOL_ALARM_DEFAULT;
          pthread_mutex_unlock(&s_vol_lock);
        }
      fclose(f);
    }
  dm_vol_apply();
}

int dm_vol_get(void)
{
  int value;
  pthread_mutex_lock(&s_vol_lock);
  value = s_vol;
  pthread_mutex_unlock(&s_vol_lock);
  return value;
}

int dm_vol_is_mute(void)
{
  int value;
  pthread_mutex_lock(&s_vol_lock);
  value = s_mute;
  pthread_mutex_unlock(&s_vol_lock);
  return value;
}

int dm_vol_alarm_get(void)
{
  int value;
  pthread_mutex_lock(&s_vol_lock);
  value = s_alarm_vol;
  pthread_mutex_unlock(&s_vol_lock);
  return value;
}

int dm_vol_alarm_set(int percent)
{
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  pthread_mutex_lock(&s_vol_lock);
  s_alarm_vol = percent;
  if (s_alarm_active > 0)
    {
      vol_apply_locked();
    }
  vol_save_locked();
  pthread_mutex_unlock(&s_vol_lock);
  return percent;
}

void dm_vol_alarm_begin(void)
{
  pthread_mutex_lock(&s_vol_lock);
  s_alarm_active++;
  if (s_alarm_active == 1)
    {
      vol_apply_locked();
    }
  pthread_mutex_unlock(&s_vol_lock);
}

void dm_vol_alarm_end(void)
{
  pthread_mutex_lock(&s_vol_lock);
  if (s_alarm_active > 0)
    {
      s_alarm_active--;
    }
  if (s_alarm_active == 0)
    {
      vol_apply_locked();
    }
  pthread_mutex_unlock(&s_vol_lock);
}

int dm_vol_set_mute(int on)
{
  int value;
  pthread_mutex_lock(&s_vol_lock);
  s_mute = on ? 1 : 0;
  value = s_mute;
  vol_apply_locked();
  vol_save_locked();
  pthread_mutex_unlock(&s_vol_lock);
  return value;
}

void dm_vol_apply(void)
{
  pthread_mutex_lock(&s_vol_lock);
  vol_apply_locked();
  pthread_mutex_unlock(&s_vol_lock);
}

int dm_vol_set(int percent)
{
  int value;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  pthread_mutex_lock(&s_vol_lock);
  s_vol = percent;
  if (s_vol > 0)
    {
      s_mute = 0;
    }
  value = s_vol;
  vol_apply_locked();
  vol_save_locked();
  pthread_mutex_unlock(&s_vol_lock);
  return value;
}

/* Short 440Hz tone for offline proactive fallback. */
int dm_vol_test_beep(void)
{
  if (dm_vol_is_mute())
    {
      return -1;
    }
  return dm_voice_beep();
}

/* Spoken sample for the Volume settings page. */
int dm_vol_test_sample(void)
{
  if (dm_vol_is_mute())
    {
      return -1;
    }
  return dm_voice_volume_test();
}

#endif /* CONFIG_DESKMATE_APP */
