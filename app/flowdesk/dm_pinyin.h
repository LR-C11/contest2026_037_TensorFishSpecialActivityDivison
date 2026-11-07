/****************************************************************************
 * dm_pinyin.h — Pinyin input method header
 ****************************************************************************/

#ifndef DM_PINYIN_H
#define DM_PINYIN_H

#ifdef __cplusplus
extern "C" {
#endif

#define PY_CAND_MAX    12
#define PY_INPUT_MAX   24
#define PY_RESULT_LEN  6

typedef struct
{
  const char *chars;
  int chars_len;
  const char *matched;
  int matched_len;
} py_cand_t;

void dm_pinyin_init(void);
void dm_pinyin_deinit(void);
int  dm_pinyin_get_cands(const char *pinyin, py_cand_t *out, int maxn);
int  dm_pinyin_get_display(const char *pinyin, char *out, int out_size);

#ifdef __cplusplus
}
#endif

#endif /* DM_PINYIN_H */
