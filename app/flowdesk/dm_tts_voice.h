/*
 * dm_tts_voice.h — selectable TTS voice profiles
 */
#ifndef DM_TTS_VOICE_H
#define DM_TTS_VOICE_H

#define DM_VOICE_NAME_MAX 16
#define DM_VOICE_ID_MAX   24

typedef struct {
  char name[DM_VOICE_NAME_MAX];   /* display */
  char api_id[DM_VOICE_ID_MAX];   /* TTS voice field */
  int pitch_hz;                   /* offline tone base */
  int offline;                    /* 1 = offline-only style */
} dm_voice_profile_t;

int dm_voice_profile_count(void);
const dm_voice_profile_t *dm_voice_profile_get(int idx);
int dm_voice_profile_sel(void);
void dm_voice_profile_set(int idx);
void dm_voice_profile_init(void); /* load /data/deskmate_voice.txt */
const char *dm_voice_profile_name(void);
const char *dm_voice_profile_api_id(void);
int dm_voice_profile_pitch(void);

#endif /* DM_TTS_VOICE_H */
