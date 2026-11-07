/*
 * dm_dict.h — BandLingo-style dictionary engine (local chunks + AI)
 */
#ifndef DM_DICT_H
#define DM_DICT_H

#include <stdbool.h>

#define DM_DICT_WORD_MAX   48
#define DM_DICT_DEF_MAX    160
#define DM_DICT_HIT_MAX    20

typedef struct {
  char word[DM_DICT_WORD_MAX];
  char def[DM_DICT_DEF_MAX];
  int score;
} dm_dict_hit_t;

void dm_dict_init(void);
int dm_dict_search(const char *query, dm_dict_hit_t *out, int max, bool chinese);
int dm_dict_lookup(const char *word, char *def, int def_cap);
const char *dm_dict_daily_word(char *def, int def_cap);
int dm_dict_total(void);

/* AI query (cloud), returns 0 on success */
int dm_dict_ai_define(const char *word, char *out, int cap);

#endif /* DM_DICT_H */
