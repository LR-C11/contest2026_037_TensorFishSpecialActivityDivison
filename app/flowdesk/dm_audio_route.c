/*
 * dm_audio_route.c — audio output channel selection
 */

#include "deskmate.h"
#include "dm_audio_route.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define OUT_CFG "/data/deskmate_aout.txt"

static int s_ch = DM_OUT_AUTO;

void dm_out_init(void)
{
  FILE *f = fopen(OUT_CFG, "r");
  int v = 0;
  if (f)
    {
      if (fscanf(f, "%d", &v) == 1)
        {
          if (v >= 0 && v <= 1)
            {
              s_ch = v;
            }
        }
      fclose(f);
    }
}

int dm_out_get(void)
{
  return s_ch;
}

void dm_out_set(int ch)
{
  FILE *f;
  if (ch < 0)
    {
      ch = 0;
    }
  if (ch > 1)
    {
      ch = 1;
    }
  s_ch = ch;
  f = fopen(OUT_CFG, "w");
  if (f)
    {
      fprintf(f, "%d\n", s_ch);
      fclose(f);
    }
}

const char *dm_out_name(int ch)
{
  switch (ch)
    {
    case DM_OUT_SPEAKER:
      return "扬声器";
    default:
      return "自动";
    }
}

#endif /* CONFIG_DESKMATE_APP */
