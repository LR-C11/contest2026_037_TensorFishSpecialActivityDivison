/*
 * dm_dict.c — local dictionary chunks (BandLingo format: word|definition)
 * Files at /resource/dict/{a-z}.txt or /data/dict/{a-z}.txt
 */

#include "deskmate.h"
#include "dm_dict.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DM_DICT_CACHE_MAX 3

typedef struct {
  char name;                 /* 'a'..'z' */
  char *data;                /* whole chunk text */
  int used;                  /* LRU stamp */
} dm_dict_cache_t;

static dm_dict_cache_t s_cache[DM_DICT_CACHE_MAX];
static int s_stamp;
static int s_ready;

static const char *chunk_path(char c, char *buf, int cap)
{
  static const char *roots[] = {
    "/resource/dict/%c.txt",
    "/data/dict/%c.txt",
  };
  FILE *f;
  int i;

  for (i = 0; i < 2; i++)
    {
      snprintf(buf, cap, roots[i], c);
      f = fopen(buf, "rb");
      if (f)
        {
          fclose(f);
          return buf;
        }
    }
  buf[0] = '\0';
  return buf;
}

void dm_dict_init(void)
{
  char path[64];
  s_ready = (chunk_path('a', path, sizeof(path))[0] != '\0');
}

static char *load_chunk(char c)
{
  char path[64];
  FILE *f;
  long sz;
  char *buf;
  int i;
  int victim = 0;

  if (c < 'a' || c > 'z')
    {
      return NULL;
    }

  for (i = 0; i < DM_DICT_CACHE_MAX; i++)
    {
      if (s_cache[i].name == c && s_cache[i].data)
        {
          s_cache[i].used = ++s_stamp;
          return s_cache[i].data;
        }
      if (s_cache[i].used < s_cache[victim].used)
        {
          victim = i;
        }
    }

  if (chunk_path(c, path, sizeof(path))[0] == '\0')
    {
      return NULL;
    }
  f = fopen(path, "rb");
  if (!f)
    {
      return NULL;
    }
  fseek(f, 0, SEEK_END);
  sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0 || sz > 2 * 1024 * 1024)
    {
      fclose(f);
      return NULL;
    }
  buf = malloc((size_t)sz + 1);
  if (!buf)
    {
      fclose(f);
      return NULL;
    }
  if (fread(buf, 1, (size_t)sz, f) != (size_t)sz)
    {
      free(buf);
      fclose(f);
      return NULL;
    }
  buf[sz] = '\0';
  fclose(f);

  if (s_cache[victim].data)
    {
      free(s_cache[victim].data);
    }
  s_cache[victim].name = c;
  s_cache[victim].data = buf;
  s_cache[victim].used = ++s_stamp;
  return buf;
}

/* Iterate lines in chunk: word|def */
static void scan_chunk(const char *data, const char *q, bool chinese,
                       dm_dict_hit_t *out, int *n, int max)
{
  const char *p = data;
  char wl[DM_DICT_WORD_MAX];
  char dl[DM_DICT_DEF_MAX];

  if (!data || !q || !q[0])
    {
      return;
    }

  while (*p && *n < max)
    {
      const char *nl = strchr(p, '\n');
      size_t linelen = nl ? (size_t)(nl - p) : strlen(p);
      const char *bar = memchr(p, '|', linelen);

      if (bar)
        {
          size_t wlen = (size_t)(bar - p);
          size_t dlen = linelen - wlen - 1;
          if (wlen > 0 && wlen < sizeof(wl) && dlen < sizeof(dl))
            {
              int score = 0;
              memcpy(wl, p, wlen);
              wl[wlen] = '\0';
              memcpy(dl, bar + 1, dlen);
              dl[dlen] = '\0';
              /* strip \r */
              if (dlen && dl[dlen - 1] == '\r')
                {
                  dl[dlen - 1] = '\0';
                }

              if (chinese)
                {
                  if (dl[0] && strstr(dl, q))
                    {
                      score = 70;
                    }
                }
              else
                {
                  char word_lc[DM_DICT_WORD_MAX];
                  size_t i;
                  for (i = 0; i < wlen; i++)
                    {
                      word_lc[i] = (char)tolower((unsigned char)wl[i]);
                    }
                  word_lc[wlen] = '\0';
                  if (strcmp(word_lc, q) == 0)
                    {
                      score = 100;
                    }
                  else if (strncmp(word_lc, q, strlen(q)) == 0)
                    {
                      score = 90;
                    }
                  else if (strstr(word_lc, q))
                    {
                      score = 70;
                    }
                }

              if (score > 0)
                {
                  int k;
                  int dup = 0;
                  for (k = 0; k < *n; k++)
                    {
                      if (strcasecmp(out[k].word, wl) == 0)
                        {
                          dup = 1;
                          break;
                        }
                    }
                  if (!dup)
                    {
                      strncpy(out[*n].word, wl, DM_DICT_WORD_MAX - 1);
                      out[*n].word[DM_DICT_WORD_MAX - 1] = '\0';
                      strncpy(out[*n].def, dl, DM_DICT_DEF_MAX - 1);
                      out[*n].def[DM_DICT_DEF_MAX - 1] = '\0';
                      out[*n].score = score;
                      (*n)++;
                    }
                }
            }
        }

      p = nl ? nl + 1 : p + linelen;
      if (!nl)
        {
          break;
        }
    }
}

static int cmp_hit(const void *a, const void *b)
{
  const dm_dict_hit_t *ha = a;
  const dm_dict_hit_t *hb = b;
  return hb->score - ha->score;
}

int dm_dict_search(const char *query, dm_dict_hit_t *out, int max, bool chinese)
{
  char q[48];
  int n = 0;
  int i;

  if (!query || !query[0] || !out || max <= 0)
    {
      return 0;
    }
  for (i = 0; query[i] && i < (int)sizeof(q) - 1; i++)
    {
      q[i] = (char)tolower((unsigned char)query[i]);
    }
  q[i] = '\0';

  if (!chinese && isalpha((unsigned char)q[0]))
    {
      /* English: only first-letter chunk */
      char *data = load_chunk(q[0]);
      if (data)
        {
          scan_chunk(data, q, false, out, &n, max);
        }
    }
  else
    {
      /* Chinese (or mixed): scan all chunks */
      char c;
      for (c = 'a'; c <= 'z' && n < max; c++)
        {
          char *data = load_chunk(c);
          if (data)
            {
              scan_chunk(data, q, true, out, &n, max);
            }
        }
    }

  if (n > 1)
    {
      qsort(out, (size_t)n, sizeof(out[0]), cmp_hit);
    }
  return n;
}

int dm_dict_lookup(const char *word, char *def, int def_cap)
{
  char w[DM_DICT_WORD_MAX];
  char *data;
  const char *p;
  size_t wl;
  int i;

  if (!word || !word[0] || !def || def_cap < 2)
    {
      return -1;
    }
  def[0] = '\0';
  for (i = 0; word[i] && i < (int)sizeof(w) - 1; i++)
    {
      w[i] = (char)tolower((unsigned char)word[i]);
    }
  w[i] = '\0';
  if (!isalpha((unsigned char)w[0]))
    {
      return -1;
    }
  data = load_chunk(w[0]);
  if (!data)
    {
      return -1;
    }
  wl = strlen(w);
  p = data;
  while (p && *p)
    {
      if (strncasecmp(p, w, wl) == 0 && p[wl] == '|')
        {
          const char *nl = strchr(p, '\n');
          size_t dlen = nl ? (size_t)(nl - (p + wl + 1)) : strlen(p + wl + 1);
          if (dlen >= (size_t)def_cap)
            {
              dlen = (size_t)def_cap - 1;
            }
          memcpy(def, p + wl + 1, dlen);
          def[dlen] = '\0';
          return 0;
        }
      p = strchr(p, '\n');
      if (p)
        {
          p++;
        }
    }
  return -1;
}

const char *dm_dict_daily_word(char *def, int def_cap)
{
  static char word[DM_DICT_WORD_MAX];
  char *data;
  char c;
  int day;
  int lines = 0;
  int pick;
  const char *p;

  if (!def || def_cap < 2)
    {
      return "";
    }
  /* simple day-of-year-ish seed */
  {
    FILE *f = fopen("/proc/uptime", "r");
    double up = 0;
    if (f)
      {
        fscanf(f, "%lf", &up);
        fclose(f);
      }
    day = (int)up / 86400;
    if (day <= 0)
      {
        day = 202; /* fixed fallback */
      }
  }
  c = (char)('a' + (day % 26));
  data = load_chunk(c);
  if (!data)
    {
      data = load_chunk('a');
    }
  if (!data)
    {
      return "";
    }

  p = data;
  while (*p)
    {
      if (*p == '\n')
        {
          lines++;
        }
      p++;
    }
  if (lines <= 0)
    {
      return "";
    }
  pick = (day * 7 + 13) % lines;

  p = data;
  {
    int seen = 0;
    while (*p && seen < pick)
      {
        if (*p == '\n')
          {
            seen++;
          }
        p++;
      }
  }
  {
    const char *bar = strchr(p, '|');
    const char *nl = strchr(p, '\n');
    if (!bar)
      {
        return "";
      }
    {
      size_t wlen = (size_t)(bar - p);
      size_t dlen = nl ? (size_t)(nl - bar - 1) : strlen(bar + 1);
      if (wlen >= sizeof(word))
        {
          wlen = sizeof(word) - 1;
        }
      memcpy(word, p, wlen);
      word[wlen] = '\0';
      if (dlen >= (size_t)def_cap)
        {
          dlen = (size_t)def_cap - 1;
        }
      memcpy(def, bar + 1, dlen);
      def[dlen] = '\0';
    }
  }
  return word;
}

int dm_dict_total(void)
{
  return 65398;
}

#endif /* CONFIG_DESKMATE_APP */
