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
#include <sys/ioctl.h>
#include <nuttx/audio/audio.h>

#define VOL_PATH "/data/deskmate_vol.txt"
#define VOL_DEFAULT 80

static int s_vol = VOL_DEFAULT;
static int s_mute;

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
  struct audio_caps_desc_s cap;
  int fd;
  int dev_vol;

  if (s_mute)
    {
      dev_vol = 0;
    }
  else
    {
      dev_vol = s_vol * 10; /* 0..100 -> 0..1000 */
      if (dev_vol > 1000)
        {
          dev_vol = 1000;
        }
    }

  fd = open("/dev/audio/pcm0p", O_WRONLY);
  if (fd < 0)
    {
      /* device may be busy; try playback node used by nxplayer */
      fd = open("/dev/audio/pcm0p", O_RDWR);
    }
  if (fd < 0)
    {
      return;
    }

  memset(&cap, 0, sizeof(cap));
  cap.caps.ac_len = sizeof(struct audio_caps_s);
  cap.caps.ac_type = AUDIO_TYPE_FEATURE;
  cap.caps.ac_format.hw = AUDIO_FU_VOLUME;
  cap.caps.ac_controls.hw[0] = (uint16_t)dev_vol;
  (void)ioctl(fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap);
  close(fd);

  /* also keep TTS helper in sync (percent, 0 when mute) */
  {
    extern void dm_voice_set_speaker_volume(int percent);
    dm_voice_set_speaker_volume(s_mute ? 0 : s_vol);
  }
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

/* short 440Hz beep ~0.25s at 16kHz mono */
int dm_vol_test_beep(void)
{
  FILE *f;
  int fd;
  int16_t buf[256];
  int i;
  int frames;
  int n;

  if (s_mute)
    {
      return -1;
    }

  fd = open("/dev/audio/pcm0p", O_WRONLY);
  if (fd < 0)
    {
      return -1;
    }

#ifdef AUDIOIOC_CONFIGURE
  {
    struct audio_caps_desc_s cap;
    memset(&cap, 0, sizeof(cap));
    cap.caps.ac_len = sizeof(struct audio_caps_s);
    cap.caps.ac_type = AUDIO_TYPE_OUTPUT;
    cap.caps.ac_channels = 1;
    cap.caps.ac_controls.hw[0] = 16000;
    cap.caps.ac_controls.b[2] = 16;
    (void)ioctl(fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap);
  }
#endif

  /* ~0.25s = 4000 samples */
  frames = 4000;
  n = 0;
  while (n < frames)
    {
      int chunk = frames - n;
      if (chunk > 256)
        {
          chunk = 256;
        }
      for (i = 0; i < chunk; i++)
        {
          /* sine approx */
          int idx = (n + i) % 36;
          static const int16_t sine[36] = {
            0, 568, 1121, 1648, 2138, 2582, 2972, 3302, 3566,
            3758, 3876, 3917, 3876, 3758, 3566, 3302, 2972, 2582,
            2138, 1648, 1121, 568, 0, -568, -1121, -1648, -2138,
            -2582, -2972, -3302, -3566, -3758, -3876, -3917, -3876, -3758
          };
          buf[i] = (int16_t)(sine[idx] / 2);
        }
      if (write(fd, buf, (size_t)chunk * 2) < 0)
        {
          break;
        }
      n += chunk;
    }
  close(fd);
  (void)f;
  return 0;
}

#endif /* CONFIG_DESKMATE_APP */
