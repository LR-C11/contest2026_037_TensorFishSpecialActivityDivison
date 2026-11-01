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
#include <nuttx/audio/audio.h>
#include <aw-alsa-lib/control.h>
#include "dm_voice.h"

#define VOL_PATH "/data/deskmate_vol.txt"
#define VOL_DEFAULT 80
#define CODEC_CARD "audiocodec"
#define DAC_VOL_MAX 160

static int s_vol = VOL_DEFAULT;
static int s_mute;

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

static void vol_save(void)
{
  FILE *f = fopen(VOL_PATH, "w");
  if (!f)
    {
      return;
    }
  fprintf(f, "%d %d\n", s_vol, s_mute);
  fclose(f);
}

void dm_vol_init(void)
{
  FILE *f = fopen(VOL_PATH, "r");
  int v = VOL_DEFAULT;
  int m = 0;

  if (f)
    {
      if (fscanf(f, "%d %d", &v, &m) >= 1)
        {
          if (v < 0)
            {
              v = 0;
            }
          if (v > 100)
            {
              v = 100;
            }
          s_vol = v;
          s_mute = m ? 1 : 0;
        }
      fclose(f);
    }
  dm_vol_apply();
}

int dm_vol_get(void)
{
  return s_vol;
}

int dm_vol_is_mute(void)
{
  return s_mute;
}

int dm_vol_set_mute(int on)
{
  s_mute = on ? 1 : 0;
  dm_vol_apply();
  vol_save();
  return s_mute;
}

void dm_vol_apply(void)
{
  unsigned int reg;
  int r1, r2;

  /* CONFIG_AUDIO_EXCLUDE_VOLUME=y strips the AUDIO_FU_VOLUME ioctl path,
     so drive DACL/R digital volume through aw-alsa-lib controls instead. */

  reg = s_mute ? 0 : vol_percent_to_reg(s_vol);
  r1 = snd_ctl_set(CODEC_CARD, "DACL digital volume", reg);
  r2 = snd_ctl_set(CODEC_CARD, "DACR digital volume", reg);
  syslog(LOG_INFO, "[dm-vol] dac vol reg=%u (dACL=%d dACR=%d)\n", reg, r1, r2);

  /* also keep TTS helper in sync (percent, 0 when mute) */
  dm_voice_set_speaker_volume(s_mute ? 0 : s_vol);
}

int dm_vol_set(int percent)
{
  if (percent < 0)
    {
      percent = 0;
    }
  if (percent > 100)
    {
      percent = 100;
    }
  s_vol = percent;
  if (s_vol > 0)
    {
      s_mute = 0;
    }
  dm_vol_apply();
  vol_save();
  return s_vol;
}

/* Short 440Hz tone for offline proactive fallback. */
int dm_vol_test_beep(void)
{
  if (s_mute)
    {
      return -1;
    }
  return dm_voice_beep();
}

/* Spoken sample for the Volume settings page. */
int dm_vol_test_sample(void)
{
  if (s_mute)
    {
      return -1;
    }
  return dm_voice_volume_test();
}

#endif /* CONFIG_DESKMATE_APP */
