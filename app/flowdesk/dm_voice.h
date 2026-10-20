/*
 * dm_voice.h — ASR / TTS + toggle mic capture
 */
#ifndef DM_VOICE_H
#define DM_VOICE_H

#include <stdbool.h>

typedef enum {
  DM_VOICE_IDLE = 0,
  DM_VOICE_REC,
  DM_VOICE_ASR,
  DM_VOICE_CHAT,
  DM_VOICE_TTS,
  DM_VOICE_PLAY,
  DM_VOICE_ERR,
} dm_voice_state_t;

int dm_voice_mic_available(void);
dm_voice_state_t dm_voice_state(void);

/* tap-to-talk: start then stop */
int dm_voice_rec_start(int max_seconds);
int dm_voice_rec_stop(void);          /* 0 = got audio */
int dm_voice_rec_active(void);

/* full pipeline on recorded WAV */
int dm_voice_asr_chat_tts(const char *wav_path, char *reply, int reply_cap);
int dm_voice_tts_play(const char *text);
int dm_voice_play_file(const char *wav_path);
void dm_voice_set_speaker_volume(int percent);
void dm_voice_reset(void);

#endif /* DM_VOICE_H */
