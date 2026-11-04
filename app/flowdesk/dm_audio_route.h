/*
 * dm_audio_route.h — output channel: speaker / auto
 */
#ifndef DM_AUDIO_ROUTE_H
#define DM_AUDIO_ROUTE_H

typedef enum {
  DM_OUT_AUTO = 0,      /* default speaker route */
  DM_OUT_SPEAKER = 1,
} dm_out_ch_t;

void dm_out_init(void);
int dm_out_get(void);
void dm_out_set(int ch);
const char *dm_out_name(int ch);

#endif /* DM_AUDIO_ROUTE_H */
