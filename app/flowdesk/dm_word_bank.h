#ifndef DM_WORD_BANK_H
#define DM_WORD_BANK_H

typedef struct
{
  const char *en;
  const char *cn;
} dm_word_t;

extern const dm_word_t g_dm_words[];
extern const int g_dm_words_n;

#endif
