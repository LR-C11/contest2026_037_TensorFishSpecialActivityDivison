/*
 * dm_audio_route.h — output channel: speaker / BT headset / auto
 */
#ifndef DM_AUDIO_ROUTE_H
#define DM_AUDIO_ROUTE_H

typedef enum {
  DM_OUT_AUTO = 0,      /* BT if connected else speaker */
  DM_OUT_SPEAKER = 1,
  DM_OUT_HEADSET = 2,
} dm_out_ch_t;

void dm_out_init(void);
int dm_out_get(void);
void dm_out_set(int ch);
const char *dm_out_name(int ch);
int dm_out_headset_present(void);

#endif /* DM_AUDIO_ROUTE_H */
