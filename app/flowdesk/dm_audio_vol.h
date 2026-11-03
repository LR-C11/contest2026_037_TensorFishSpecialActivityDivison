/*
 * dm_audio_vol.h — speaker / media volume (R528 codec via NuttX audio)
 */
#ifndef DM_AUDIO_VOL_H
#define DM_AUDIO_VOL_H

int dm_vol_get(void);              /* 0..100 */
int dm_vol_set(int percent);       /* 0..100, persists */
int dm_vol_is_mute(void);
int dm_vol_set_mute(int on);
int dm_vol_alarm_get(void);        /* independent alarm volume, 0..100 */
int dm_vol_alarm_set(int percent);
void dm_vol_alarm_begin(void);     /* temporarily route alarm volume to speaker */
void dm_vol_alarm_end(void);       /* restore media volume after alarm speech */
void dm_vol_apply(void);           /* push current vol to audio device */
int dm_vol_test_beep(void);        /* short offline fallback tone */
int dm_vol_test_sample(void);      /* spoken settings sample */
void dm_vol_init(void);

#endif /* DM_AUDIO_VOL_H */
