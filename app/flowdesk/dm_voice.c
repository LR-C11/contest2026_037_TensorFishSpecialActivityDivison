/*
 * dm_voice.c — tap-to-talk voice pipeline + speaker playback
 *
 * Flow: tap1 record -> tap2 stop -> ASR -> LLM -> TTS -> speaker
 * Audio nodes: /dev/audio/pcm0c (mic), /dev/audio/pcm0p (4R 3W speaker)
 * Do not modify audio drivers.
 */

#include "deskmate.h"
#include "dm_voice.h"
#include "dm_tts_voice.h"
#include <system/nxplayer.h>
#include <system/nxrecorder.h>
#include "dm_audio_route.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <syslog.h>
#include <nuttx/audio/audio.h>
#include <netutils/cJSON.h>

#define VOICE_BASE   "https://token-plan-ams.xiaomimimo.com/v1"
#define MIMO_KEY_PATH "/data/deskmate_mimo.key"
#define MIMO_CURL_CONFIG "/tmp/dm_voice_mimo.conf"
#define ASR_MODEL    "mimo-v2.5-asr"
#define TTS_MODEL    "mimo-v2.5-tts"
#define CHAT_MODEL   "mimo-v2.6-flash"

#define WAV_REC      "/tmp/dm_voice_in.wav"
#define WAV_ASR      "/tmp/dm_voice_asr_input.wav"
#define WAV_TTS      "/tmp/dm_voice_out.wav"
#define WAV_VOL_TEST_FMT "/data/deskmate_volume_test_%d.wav"
#define PCM_RAW      "/tmp/dm_play.raw"
#define TXT_ASR      "/tmp/dm_voice_asr.txt"
#define TXT_CHAT     "/tmp/dm_voice_chat.txt"
#define JSON_REQ     "/tmp/dm_voice_req.json"
#define VOICE_CURL_SCRIPT "/tmp/dm_voice_curl.sh"

#define REC_RATE     8000
#define REC_BITS     16
#define REC_CH       1
#define REC_MAX_SEC  8

static volatile dm_voice_state_t s_st = DM_VOICE_IDLE;
static volatile int s_rec_active;
static pthread_mutex_t s_rec_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_audio_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_voice_api_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_speaker_vol = 80; /* percent */
static volatile dm_voice_failure_t s_last_failure = DM_VOICE_FAILURE_NONE;

dm_voice_failure_t dm_voice_last_failure(void)
{
  return s_last_failure;
}

dm_voice_state_t dm_voice_state(void)
{
  return s_st;
}

void dm_voice_reset(void)
{
  (void)dm_voice_rec_stop();
  s_st = DM_VOICE_IDLE;
}

int dm_voice_mic_available(void)
{
  int fd = open("/dev/audio/pcm0c", O_RDONLY);
  if (fd < 0)
    {
      return 0;
    }
  close(fd);
  return 1;
}

int dm_voice_rec_active(void)
{
  return s_rec_active;
}

/* ---------- recorder: nxrecorder (buffer-queue API) ----------
 * plain read() on /dev/audio/pcm0c is a no-op on this driver (no .read
 * op), only the nxrecorder buffer-queue path actually captures audio.
 */

static struct nxrecorder_s *s_rec;

static void *rec_auto_stop(void *arg)
{
  int max_sec = (int)(intptr_t)arg;

  sleep(max_sec > 0 ? max_sec : REC_MAX_SEC);
  (void)dm_voice_rec_stop();
  return NULL;
}

int dm_voice_rec_start(int max_seconds)
{
  pthread_t th;
  pthread_attr_t attr;

  pthread_mutex_lock(&s_rec_lock);
  if (s_rec_active)
    {
      pthread_mutex_unlock(&s_rec_lock);
      return 0;
    }

  /* Do not contend for the audio DMA while playback is active.  Use
   * trylock so a UI click never waits behind a long speaker operation. */
  if (pthread_mutex_trylock(&s_audio_lock) != 0)
    {
      pthread_mutex_unlock(&s_rec_lock);
      return -EBUSY;
    }

  unlink(WAV_REC);
  s_rec = nxrecorder_create();
  if (!s_rec)
    {
      pthread_mutex_unlock(&s_audio_lock);
      pthread_mutex_unlock(&s_rec_lock);
      s_st = DM_VOICE_ERR;
      return -1;
    }
  (void)nxrecorder_setdevice(s_rec, "/dev/audio/pcm0c");
  if (nxrecorder_recordinternal(s_rec, WAV_REC, AUDIO_FMT_PCM,
                                REC_CH, REC_BITS, REC_RATE, 1) != OK)
    {
      nxrecorder_release(s_rec);
      s_rec = NULL;
      pthread_mutex_unlock(&s_audio_lock);
      pthread_mutex_unlock(&s_rec_lock);
      s_st = DM_VOICE_ERR;
      return -1;
    }

  s_rec_active = 1;
  s_st = DM_VOICE_REC;
  pthread_mutex_unlock(&s_audio_lock);
  pthread_mutex_unlock(&s_rec_lock);

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 4096);
  if (pthread_create(&th, &attr, rec_auto_stop,
                     (void *)(intptr_t)(max_seconds > 0 ? max_seconds
                                                        : REC_MAX_SEC)) == 0)
    {
      pthread_detach(th);
    }
  pthread_attr_destroy(&attr);
  return 0;
}

int dm_voice_rec_stop(void)
{
  FILE *f;
  long size;

  pthread_mutex_lock(&s_rec_lock);
  if (s_rec_active)
    {
      /* Stop/join can wait for the NuttX audio worker.  Keep this off the
       * LVGL event thread and hold the audio gate until the recorder releases
       * its DMA/session resources. */
      pthread_mutex_lock(&s_audio_lock);
      if (s_rec)
        {
          nxrecorder_stop(s_rec);
          nxrecorder_release(s_rec);
          s_rec = NULL;
        }
      s_rec_active = 0;
      s_st = DM_VOICE_IDLE;
      pthread_mutex_unlock(&s_audio_lock);
    }
  pthread_mutex_unlock(&s_rec_lock);

  f = fopen(WAV_REC, "rb");
  if (!f)
    {
      syslog(LOG_ERR, "[dm-voice] recording file missing\n");
      s_st = DM_VOICE_ERR;
      return -1;
    }

  if (fseek(f, 0, SEEK_END) != 0)
    {
      fclose(f);
      s_st = DM_VOICE_ERR;
      return -1;
    }

  size = ftell(f);
  fclose(f);
  if (size <= 44)
    {
      syslog(LOG_ERR, "[dm-voice] recording has no audio payload\n");
      s_st = DM_VOICE_ERR;
      return -1;
    }

  return 0;
}



/* ---- player: nxplayer (buffer-queue API) ---- */

/* The audio driver owns a small number of DMA channels.  Serialize all
 * playback and recording setup/teardown so the app never starts a new stream
 * while an earlier stream is still releasing its buffers. */

/* nxplayer treats .wav as raw PCM and never reads the WAV header
 * (its PCM ops->pre_parse is NULL), so the fmt chunk is parsed here and
 * the payload is fed via nxplayer_playraw() with explicit parameters. */
static int play_wav(const char *path)
{
  const char *src = (path && path[0]) ? path : WAV_TTS;
  struct nxplayer_s *p = NULL;
  FILE *f = NULL;
  FILE *o = NULL;
  uint8_t hdr[44];
  char buf[1024];
  size_t n;
  size_t raw_len = 0;
  uint32_t rate;
  uint16_t ch;
  uint16_t bits;
  uint64_t bytes_per_sec;
  uint64_t play_ms;
  int rc = -1;

  if (pthread_mutex_trylock(&s_audio_lock) != 0)
    {
      return -EBUSY;
    }
  if (s_rec_active)
    {
      pthread_mutex_unlock(&s_audio_lock);
      return -EBUSY;
    }

  p = nxplayer_create();
  if (!p || nxplayer_setdevice(p, "/dev/audio/pcm0p") != OK)
    {
      goto out;
    }

  f = fopen(src, "rb");
  if (!f)
    {
      goto out;
    }
  if (fread(hdr, 1, 44, f) != 44 || memcmp(hdr, "RIFF", 4) != 0)
    {
      goto out;
    }
  ch = (uint16_t)(hdr[22] | (hdr[23] << 8));
  rate = (uint32_t)(hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) |
                    ((uint32_t)hdr[27] << 24));
  bits = (uint16_t)(hdr[34] | (hdr[35] << 8));
  if (rate < 8000 || rate > 48000)
    {
      rate = 24000;
    }
  if (ch < 1 || ch > 2)
    {
      ch = 1;
    }
  if (bits != 8 && bits != 16)
    {
      bits = 16;
    }

  o = fopen(PCM_RAW, "wb");
  if (!o)
    {
      goto out;
    }
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
      if (fwrite(buf, 1, n, o) != n)
        {
          goto out;
        }
      raw_len += n;
    }
  if (ferror(f) || raw_len == 0)
    {
      goto out;
    }
  fclose(o);
  o = NULL;
  fclose(f);
  f = NULL;

  if (nxplayer_playraw(p, PCM_RAW, AUDIO_FMT_PCM, 0, (uint8_t)ch,
                       (uint8_t)bits, rate, 1) != OK)
    {
      goto out;
    }

  /* nxplayer_playraw() returns as soon as its worker is started.  Keep the
   * audio gate until the PCM duration elapses and stop/join the worker, so a
   * following capture/playback cannot race the previous DMA release. */
  bytes_per_sec = (uint64_t)rate * ch * bits / 8;
  play_ms = bytes_per_sec ? ((uint64_t)raw_len * 1000 / bytes_per_sec) : 1000;
  play_ms += 150;
  if (play_ms > 30000)
    {
      play_ms = 30000;
    }
  while (play_ms >= 1000)
    {
      sleep(1);
      play_ms -= 1000;
    }
  if (play_ms > 0)
    {
      usleep((useconds_t)(play_ms * 1000));
    }

  (void)nxplayer_stop(p);
  rc = 0;

out:
  if (o)
    {
      fclose(o);
    }
  if (f)
    {
      fclose(f);
    }
  if (p)
    {
      nxplayer_release(p);
    }
  pthread_mutex_unlock(&s_audio_lock);
  return rc;
}

/* Play one short 440Hz test tone.  This is synchronous by design; callers
 * run it on a worker so neither LVGL nor the recorder UI can block. */
int dm_voice_beep(void)
{
  struct nxplayer_s *p = NULL;
  int rc = -1;

  if (pthread_mutex_trylock(&s_audio_lock) != 0)
    {
      return -EBUSY;
    }
  if (s_rec_active)
    {
      pthread_mutex_unlock(&s_audio_lock);
      return -EBUSY;
    }

  p = nxplayer_create();
  if (!p || nxplayer_setdevice(p, "/dev/audio/pcm0p") != OK)
    {
      goto out;
    }

  if (nxplayer_playtone(p, 16000, 440, 1) != OK)
    {
      goto out;
    }

  /* Tone playback is asynchronous; wait slightly beyond its duration and
   * join it before allowing the next audio operation. */
  usleep(1150 * 1000);
  (void)nxplayer_stop(p);
  rc = 0;

out:
  if (p)
    {
      nxplayer_release(p);
    }
  pthread_mutex_unlock(&s_audio_lock);
  return rc;
}


/* ---- HTTP / ASR / LLM / TTS (sync, run in worker thread) ---- */

void dm_voice_set_speaker_volume(int percent)
{
  if (percent < 0)
    {
      percent = 0;
    }
  if (percent > 100)
    {
      percent = 100;
    }
  s_speaker_vol = percent;
}

static int json_escape(const char *in, char *out, size_t cap)
{
  size_t o = 0;
  size_t i;

  if (!in || !out || cap < 2)
    {
      if (out && cap)
        {
          out[0] = '\0';
        }
      return -1;
    }

  for (i = 0; in[i]; i++)
    {
      unsigned char ch = (unsigned char)in[i];
      size_t need = (ch == '"' || ch == '\\' || ch == '\n') ? 2 :
                    (ch >= 0x20 ? 1 : 0);

      if (o + need >= cap)
        {
          out[o] = '\0';
          return -1;
        }
      if (ch == '"' || ch == '\\')
        {
          out[o++] = '\\';
          out[o++] = (char)ch;
        }
      else if (ch == '\n')
        {
          out[o++] = '\\';
          out[o++] = 'n';
        }
      else if (ch >= 0x20)
        {
          out[o++] = (char)ch;
        }
    }
  out[o] = '\0';
  return (int)o;
}

/* Load and validate the key supplied at runtime. Never log or compile it. */
static int voice_key_load(char *key, size_t key_cap)
{
  FILE *f;
  size_t i;
  size_t n;
  int valid = 0;

  if (!key || key_cap < 16)
    {
      return 0;
    }
  key[0] = '\0';
  f = fopen(MIMO_KEY_PATH, "rb");
  if (!f)
    {
      return 0;
    }

  if (fgets(key, (int)key_cap, f))
    {
      n = strlen(key);
      while (n > 0 && (key[n - 1] == '\r' || key[n - 1] == '\n' ||
                       key[n - 1] == ' ' || key[n - 1] == '\t'))
        {
          key[--n] = '\0';
        }
      valid = n > 8 && n < key_cap - 1;
      for (i = 0; valid && i < n; i++)
        {
          unsigned char ch = (unsigned char)key[i];
          if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
                ch == '.'))
            {
              valid = 0;
            }
        }
    }

  fclose(f);
  if (!valid)
    {
      memset(key, 0, key_cap);
    }
  return valid;
}

/* Curl reads a short-lived, owner-only config file. The shell script itself
 * never contains the key and does not depend on NSH command substitution. */
static int run_curl_script(const char *line)
{
  char key[128];
  FILE *kf;
  FILE *sf;
  int key_write_ok;
  int key_close_rc;
  int script_write_ok;
  int script_close_rc;
  int rc;

  if (!voice_key_load(key, sizeof(key)))
    {
      s_last_failure = DM_VOICE_FAILURE_API_KEY;
      syslog(LOG_ERR, "[dm-voice] runtime API key missing or invalid\n");
      return -1;
    }

  kf = fopen(MIMO_CURL_CONFIG, "w");
  if (!kf || chmod(MIMO_CURL_CONFIG, S_IRUSR | S_IWUSR) != 0)
    {
      if (kf)
        {
          fclose(kf);
        }
      memset(key, 0, sizeof(key));
      unlink(MIMO_CURL_CONFIG);
      return -1;
    }
  key_write_ok = fprintf(kf, "header = \"Authorization: Bearer %s\"\n",
                         key) > 0;
  key_close_rc = fclose(kf);
  memset(key, 0, sizeof(key));
  if (!key_write_ok || key_close_rc != 0)
    {
      unlink(MIMO_CURL_CONFIG);
      return -1;
    }

  sf = fopen(VOICE_CURL_SCRIPT, "w");
  if (!sf)
    {
      unlink(MIMO_CURL_CONFIG);
      syslog(LOG_ERR, "[dm-voice] cannot create curl script\n");
      return -1;
    }

  script_write_ok = fputs("#!/bin/sh\n", sf) != EOF &&
                    fputs(line, sf) != EOF && fputc('\n', sf) != EOF;
  script_close_rc = fclose(sf);
  if (!script_write_ok || script_close_rc != 0)
    {
      unlink(MIMO_CURL_CONFIG);
      syslog(LOG_ERR, "[dm-voice] cannot write curl script\n");
      return -1;
    }

  rc = system("sh " VOICE_CURL_SCRIPT);
  unlink(MIMO_CURL_CONFIG);
  if (rc != 0)
    {
      syslog(LOG_ERR, "[dm-voice] curl script failed status=%d\n", rc);
      return -1;
    }
  return 0;
}


static const char b64tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_encode(const unsigned char *in, size_t inlen, char *out,
                      size_t outcap)
{
  size_t i = 0;
  size_t o = 0;

  if (!in || !out || outcap < 2 || inlen > (outcap - 1) / 4 * 3)
    {
      return -1;
    }

  while (i < inlen)
    {
      size_t remain = inlen - i;
      unsigned int v = (unsigned int)in[i++] << 16;
      if (remain > 1)
        {
          v |= (unsigned int)in[i++] << 8;
        }
      if (remain > 2)
        {
          v |= (unsigned int)in[i++];
        }
      out[o++] = b64tab[(v >> 18) & 63];
      out[o++] = b64tab[(v >> 12) & 63];
      out[o++] = remain > 1 ? b64tab[(v >> 6) & 63] : '=';
      out[o++] = remain > 2 ? b64tab[v & 63] : '=';
    }
  out[o] = '\0';
  return (int)o;
}

static int b64_decode(const char *in, unsigned char *out, size_t outcap)
{
  uint32_t val = 0;
  int valb = -8;
  size_t o = 0;
  for (; *in && *in != '=' && o < outcap; in++)
    {
      const char *p = strchr(b64tab, *in);
      if (!p) continue;
      val = (val << 6) + (int)(p - b64tab);
      valb += 6;
      if (valb >= 0)
        {
          out[o++] = (unsigned char)((val >> valb) & 255);
          valb -= 8;
        }
    }
  return (int)o;
}

static int http_post_json(const char *path, const char *json_body,
                          const char *out_path)
{
  char cmd[1400];
  FILE *f;
  size_t body_len;
  int write_ok;
  int close_rc;
  int rc;

  if (!dm_wifi_connected())
    {
      s_last_failure = DM_VOICE_FAILURE_CHAT;
      return -1;
    }
  if (dm_wifi_wait_time_sync(40) != 0)
    {
      s_last_failure = DM_VOICE_FAILURE_TIME;
      syslog(LOG_WARNING, "[dm-voice] secure request deferred: clock not ready\n");
      return -1;
    }

  f = fopen(JSON_REQ, "w");
  if (!f)
    {
      s_last_failure = DM_VOICE_FAILURE_CHAT;
      return -1;
    }
  body_len = strlen(json_body);
  write_ok = fwrite(json_body, 1, body_len, f) == body_len;
  close_rc = fclose(f);
  if (!write_ok || close_rc != 0)
    {
      s_last_failure = DM_VOICE_FAILURE_CHAT;
      return -1;
    }
  unlink(out_path);
  /* Use short curl options supported by the board build.  The runtime key is
   * expanded by the shell and is not written into this script. */
  snprintf(cmd, sizeof(cmd),
           "curl -K " MIMO_CURL_CONFIG " -s -S -f -m 25 -X POST %s%s "
           "-H 'Content-Type: application/json' "
           "-d @%s -o %s",
           VOICE_BASE, path, JSON_REQ, out_path);
  rc = run_curl_script(cmd);
  if (rc != 0 && s_last_failure == DM_VOICE_FAILURE_NONE)
    {
      s_last_failure = DM_VOICE_FAILURE_CHAT;
    }
  return rc;
}

static cJSON *voice_json_load(const char *path, long max_size)
{
  FILE *f;
  char *buf;
  long size;
  cJSON *root;

  f = fopen(path, "rb");
  if (!f)
    {
      return NULL;
    }
  if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) <= 0 ||
      size > max_size || fseek(f, 0, SEEK_SET) != 0)
    {
      fclose(f);
      return NULL;
    }
  buf = malloc((size_t)size + 1);
  if (!buf)
    {
      fclose(f);
      return NULL;
    }
  if (fread(buf, 1, (size_t)size, f) != (size_t)size)
    {
      free(buf);
      fclose(f);
      return NULL;
    }
  fclose(f);
  buf[size] = '\0';
  root = cJSON_Parse(buf);
  free(buf);
  return root;
}

static cJSON *voice_first_message(cJSON *root)
{
  cJSON *choices;
  cJSON *choice;

  if (!root)
    {
      return NULL;
    }
  choices = cJSON_GetObjectItem(root, "choices");
  if (!choices || !cJSON_IsArray(choices))
    {
      return NULL;
    }
  choice = cJSON_GetArrayItem(choices, 0);
  return choice ? cJSON_GetObjectItem(choice, "message") : NULL;
}

static void put_le16(uint8_t *p, uint16_t value)
{
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *p, uint32_t value)
{
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
  p[2] = (uint8_t)(value >> 16);
  p[3] = (uint8_t)(value >> 24);
}

/* nxrecorder's AUDIO_FMT_PCM encoder writes headerless PCM even when the
 * filename ends in .wav.  Wrap that payload in a standards-compliant WAV
 * header before sending `format: wav` to the MiMo chat-completions ASR API. */
static int voice_prepare_wav(const char *src, const char **wav_out)
{
  FILE *in;
  FILE *out;
  uint8_t header[44];
  char buf[1024];
  long size;
  size_t n;
  uint32_t data_size;
  unsigned int byte_rate = REC_RATE * REC_CH * REC_BITS / 8;
  int rc = -1;

  if (!src || !wav_out)
    {
      return -1;
    }

  *wav_out = src;
  in = fopen(src, "rb");
  if (!in)
    {
      return -1;
    }

  if (fread(header, 1, 12, in) == 12 &&
      memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WAVE", 4) == 0)
    {
      fclose(in);
      return 0;
    }

  if (fseek(in, 0, SEEK_END) != 0 || (size = ftell(in)) <= 0 ||
      size > 2 * 1024 * 1024 - 44 || fseek(in, 0, SEEK_SET) != 0)
    {
      fclose(in);
      return -1;
    }

  data_size = (uint32_t)size;
  memset(header, 0, sizeof(header));
  memcpy(header, "RIFF", 4);
  put_le32(header + 4, data_size + 36);
  memcpy(header + 8, "WAVEfmt ", 8);
  put_le32(header + 16, 16);
  put_le16(header + 20, 1);
  put_le16(header + 22, REC_CH);
  put_le32(header + 24, REC_RATE);
  put_le32(header + 28, byte_rate);
  put_le16(header + 32, REC_CH * REC_BITS / 8);
  put_le16(header + 34, REC_BITS);
  memcpy(header + 36, "data", 4);
  put_le32(header + 40, data_size);

  out = fopen(WAV_ASR, "wb");
  if (!out)
    {
      fclose(in);
      return -1;
    }

  if (fwrite(header, 1, sizeof(header), out) == sizeof(header))
    {
      rc = 0;
      while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        {
          if (fwrite(buf, 1, n, out) != n)
            {
              rc = -1;
              break;
            }
        }
      if (ferror(in))
        {
          rc = -1;
        }
    }

  if (fclose(out) != 0)
    {
      rc = -1;
    }
  fclose(in);
  if (rc == 0)
    {
      *wav_out = WAV_ASR;
    }
  else
    {
      unlink(WAV_ASR);
    }
  return rc;
}

static int asr_wav(const char *wav, char *text, int cap)
{
  FILE *f;
  unsigned char *raw;
  char *b64;
  char *body;
  size_t body_cap;
  long sz;
  int n;
  cJSON *root;
  cJSON *message;
  cJSON *content;

  if (!wav || !text || cap < 2)
    {
      return -1;
    }
  text[0] = '\0';
  f = fopen(wav, "rb");
  if (!f)
    {
      return -1;
    }
  if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) <= 44 ||
      sz > 2 * 1024 * 1024 || fseek(f, 0, SEEK_SET) != 0)
    {
      fclose(f);
      return -1;
    }
  raw = malloc((size_t)sz);
  b64 = malloc((size_t)sz * 2 + 8);
  body_cap = (size_t)sz * 2 + 512;
  body = malloc(body_cap);
  if (!raw || !b64 || !body)
    {
      free(raw);
      free(b64);
      free(body);
      fclose(f);
      return -1;
    }
  if (fread(raw, 1, (size_t)sz, f) != (size_t)sz)
    {
      free(raw);
      free(b64);
      free(body);
      fclose(f);
      return -1;
    }
  fclose(f);
  if (b64_encode(raw, (size_t)sz, b64, (size_t)sz * 2 + 8) <= 0)
    {
      free(raw);
      free(b64);
      free(body);
      return -1;
    }
  free(raw);

  /* Official MiMo ASR schema: data URL + one input_audio content part. */
  n = snprintf(body, body_cap,
               "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":"
               "[{\"type\":\"input_audio\",\"input_audio\":{\"data\":"
               "\"data:audio/wav;base64,%s\"}}]}],"
               "\"asr_options\":{\"language\":\"auto\"}}",
               ASR_MODEL, b64);
  free(b64);
  if (n <= 0 || (size_t)n >= body_cap)
    {
      free(body);
      return -1;
    }
  if (http_post_json("/chat/completions", body, TXT_ASR) != 0)
    {
      free(body);
      return -1;
    }
  free(body);

  root = voice_json_load(TXT_ASR, 256 * 1024);
  if (!root)
    {
      return -1;
    }
  message = voice_first_message(root);
  content = message ? cJSON_GetObjectItem(message, "content") : NULL;
  if (content && cJSON_IsString(content) && content->valuestring)
    {
      snprintf(text, (size_t)cap, "%s", content->valuestring);
    }
  cJSON_Delete(root);
  return text[0] ? 0 : -1;
}

static int chat_reply(const char *user, char *out, int cap)
{
  char esc[400];
  char hist[1400];
  char body[2400];
  int n;
  cJSON *root;
  cJSON *message;
  cJSON *content;

  if (!user || !out || cap < 2)
    {
      return -1;
    }
  out[0] = '\0';
  if (json_escape(user, esc, sizeof(esc)) < 0)
    {
      return -1;
    }
  dm_chat_hist_json(hist, sizeof(hist));

  n = snprintf(body, sizeof(body),
               "{\"model\":\"%s\",\"messages\":["
               "{\"role\":\"system\",\"content\":\"你是openvela语音小助手，口语化简短回答，不超过60字。\"},"
               "%s"
               "{\"role\":\"user\",\"content\":\"%s\"}],"
               "\"max_completion_tokens\":400,\"thinking\":{\"type\":\"disabled\"}}",
               CHAT_MODEL, hist, esc);
  if (n <= 0 || n >= (int)sizeof(body))
    {
      return -1;
    }
  if (http_post_json("/chat/completions", body, TXT_CHAT) != 0)
    {
      return -1;
    }

  root = voice_json_load(TXT_CHAT, 64 * 1024);
  if (!root)
    {
      return -1;
    }
  message = voice_first_message(root);
  content = message ? cJSON_GetObjectItem(message, "content") : NULL;
  if (content && cJSON_IsString(content) && content->valuestring)
    {
      snprintf(out, (size_t)cap, "%s", content->valuestring);
    }
  cJSON_Delete(root);

  if (out[0])
    {
      dm_chat_hist_push(user, out);
      return 0;
    }
  return -1;
}

static int tts_synth(const char *text, const char *wav_path,
                     const char *voice_id)
{
  char esc[400];
  char voice_esc[64];
  char body[1000];
  unsigned char *raw = NULL;
  FILE *f = NULL;
  long sz;
  int n;
  int rawlen;
  int write_ok;
  int close_rc;
  int rc = -1;
  cJSON *root = NULL;
  cJSON *message;
  cJSON *audio;
  cJSON *data;

  if (!text || !text[0] || !wav_path || !voice_id || !voice_id[0])
    {
      return -1;
    }
  if (json_escape(text, esc, sizeof(esc)) < 0 ||
      json_escape(voice_id, voice_esc, sizeof(voice_esc)) < 0)
    {
      return -1;
    }
  n = snprintf(body, sizeof(body),
               "{\"model\":\"%s\",\"messages\":["
               "{\"role\":\"user\",\"content\":\"请使用指定音色，以自然清晰的语气朗读。\"},"
               "{\"role\":\"assistant\",\"content\":\"%s\"}],"
               "\"audio\":{\"format\":\"wav\",\"voice\":\"%s\"}}",
               TTS_MODEL, esc, voice_esc);
  if (n <= 0 || n >= (int)sizeof(body))
    {
      return -1;
    }
  if (http_post_json("/chat/completions", body, TXT_CHAT) != 0)
    {
      return -1;
    }

  root = voice_json_load(TXT_CHAT, 4 * 1024 * 1024);
  if (!root)
    {
      goto out;
    }
  message = voice_first_message(root);
  audio = message ? cJSON_GetObjectItem(message, "audio") : NULL;
  data = audio ? cJSON_GetObjectItem(audio, "data") : NULL;
  if (!data || !cJSON_IsString(data) || !data->valuestring)
    {
      goto out;
    }

  sz = (long)strlen(data->valuestring);
  if (sz <= 0 || sz > 4 * 1024 * 1024)
    {
      goto out;
    }
  raw = malloc((size_t)sz);
  if (!raw)
    {
      goto out;
    }
  rawlen = b64_decode(data->valuestring, raw, (size_t)sz);
  if (rawlen < 44 || memcmp(raw, "RIFF", 4) != 0 ||
      memcmp(raw + 8, "WAVE", 4) != 0)
    {
      goto out;
    }

  f = fopen(wav_path, "wb");
  if (!f)
    {
      goto out;
    }
  write_ok = fwrite(raw, 1, (size_t)rawlen, f) == (size_t)rawlen;
  close_rc = fclose(f);
  f = NULL;
  if (write_ok && close_rc == 0)
    {
      rc = 0;
    }

out:
  if (f)
    {
      fclose(f);
    }
  free(raw);
  if (root)
    {
      cJSON_Delete(root);
    }
  if (rc != 0)
    {
      unlink(wav_path);
      if (s_last_failure == DM_VOICE_FAILURE_NONE)
        {
          s_last_failure = DM_VOICE_FAILURE_TTS;
        }
    }
  return rc;
}

static int voice_asr_chat_tts_unlocked(const char *wav_path, char *reply,
                                       int cap)
{
  char text[256];
  const char *asr_path;
  int play_rc = -1;

  if (!wav_path || !reply || cap < 8)
    {
      return -1;
    }
  s_last_failure = DM_VOICE_FAILURE_NONE;
  reply[0] = '\0';

  if (voice_prepare_wav(wav_path, &asr_path) != 0)
    {
      s_last_failure = DM_VOICE_FAILURE_ASR;
      s_st = DM_VOICE_ERR;
      return -1;
    }

  s_st = DM_VOICE_ASR;
  if (asr_wav(asr_path, text, sizeof(text)) != 0)
    {
      if (s_last_failure == DM_VOICE_FAILURE_NONE)
        {
          s_last_failure = DM_VOICE_FAILURE_ASR;
        }
      s_st = DM_VOICE_ERR;
      return -1;
    }

  s_st = DM_VOICE_CHAT;
  if (chat_reply(text, reply, cap) != 0)
    {
      if (s_last_failure == DM_VOICE_FAILURE_NONE)
        {
          s_last_failure = DM_VOICE_FAILURE_CHAT;
        }
      strncpy(reply, "（网络忙，没听清）", (size_t)cap - 1);
      reply[cap - 1] = '\0';
      s_st = DM_VOICE_ERR;
      return -1;
    }

  /* Preserve the text answer even if playback fails; the caller can show
   * that the reply is text-only rather than pretending the speaker worked. */
  s_st = DM_VOICE_TTS;
  if (tts_synth(reply, WAV_TTS, dm_voice_profile_api_id()) == 0)
    {
      s_st = DM_VOICE_PLAY;
      play_rc = play_wav(WAV_TTS);
    }
  else if (s_last_failure == DM_VOICE_FAILURE_NONE)
    {
      s_last_failure = DM_VOICE_FAILURE_TTS;
    }
  s_st = DM_VOICE_IDLE;
  return play_rc == 0 ? 0 : 1;
}

int dm_voice_asr_chat_tts(const char *wav_path, char *reply, int cap)
{
  int rc;

  pthread_mutex_lock(&s_voice_api_lock);
  rc = voice_asr_chat_tts_unlocked(wav_path, reply, cap);
  pthread_mutex_unlock(&s_voice_api_lock);
  return rc;
}

static int voice_tts_play_unlocked(const char *text)
{
  s_last_failure = DM_VOICE_FAILURE_NONE;
  if (!text || !text[0])
    {
      return -1;
    }
  s_st = DM_VOICE_TTS;
  if (tts_synth(text, WAV_TTS, dm_voice_profile_api_id()) != 0)
    {
      if (s_last_failure == DM_VOICE_FAILURE_NONE)
        {
          s_last_failure = DM_VOICE_FAILURE_TTS;
        }
      s_st = DM_VOICE_ERR;
      return -1;
    }
  s_st = DM_VOICE_PLAY;
  if (play_wav(WAV_TTS) != 0)
    {
      s_last_failure = DM_VOICE_FAILURE_TTS;
      s_st = DM_VOICE_ERR;
      return -1;
    }
  s_st = DM_VOICE_IDLE;
  return 0;
}

int dm_voice_tts_play(const char *text)
{
  int rc;

  pthread_mutex_lock(&s_voice_api_lock);
  rc = voice_tts_play_unlocked(text);
  pthread_mutex_unlock(&s_voice_api_lock);
  return rc;
}

static int voice_wav_cache_valid(const char *path)
{
  FILE *f = fopen(path, "rb");
  unsigned char hdr[44];
  long size;
  int valid = 0;

  if (!f)
    {
      return 0;
    }
  if (fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) > 44 &&
      fseek(f, 0, SEEK_SET) == 0 &&
      fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr) &&
      memcmp(hdr, "RIFF", 4) == 0 && memcmp(hdr + 8, "WAVE", 4) == 0)
    {
      valid = 1;
    }
  fclose(f);
  return valid;
}

/* Generate each MiMo preset preview once and retain it on /data for offline
 * auditioning after the first successful cloud synthesis. */
static int voice_profile_preview_unlocked(int profile_idx)
{
  static const char sample_text[] =
      "你好，我是你的语音助手。这是当前音色试听。";
  const dm_voice_profile_t *profile;
  char path[64];

  if (profile_idx < 0 || profile_idx >= dm_voice_profile_count())
    {
      return -1;
    }
  profile = dm_voice_profile_get(profile_idx);
  snprintf(path, sizeof(path), "/data/dm_voice_sample_%d.wav", profile_idx);
  s_last_failure = DM_VOICE_FAILURE_NONE;
  if (!voice_wav_cache_valid(path))
    {
      unlink(path);
      s_st = DM_VOICE_TTS;
      if (tts_synth(sample_text, path, profile->api_id) != 0 ||
          !voice_wav_cache_valid(path))
        {
          if (s_last_failure == DM_VOICE_FAILURE_NONE)
            {
              s_last_failure = DM_VOICE_FAILURE_TTS;
            }
          s_st = DM_VOICE_ERR;
          return -1;
        }
    }

  s_st = DM_VOICE_PLAY;
  if (play_wav(path) != 0)
    {
      s_last_failure = DM_VOICE_FAILURE_TTS;
      s_st = DM_VOICE_ERR;
      return -1;
    }
  s_st = DM_VOICE_IDLE;
  return 0;
}

int dm_voice_profile_preview(int profile_idx)
{
  int rc;

  pthread_mutex_lock(&s_voice_api_lock);
  rc = voice_profile_preview_unlocked(profile_idx);
  pthread_mutex_unlock(&s_voice_api_lock);
  return rc;
}

/* First volume test creates a speech sample with the selected MiMo preset and
 * caches it in /data; later tests replay without another cloud request. */
static int voice_volume_test_unlocked(void)
{
  static const char sample_text[] = "音量试听。请确认这个音量听起来是否舒适。";
  char path[64];
  int profile_idx = dm_voice_profile_sel();

  if (profile_idx < 0 || profile_idx >= dm_voice_profile_count())
    {
      profile_idx = 0;
    }
  snprintf(path, sizeof(path), WAV_VOL_TEST_FMT, profile_idx);
  s_last_failure = DM_VOICE_FAILURE_NONE;
  if (!voice_wav_cache_valid(path))
    {
      unlink(path);
      if (tts_synth(sample_text, path, dm_voice_profile_api_id()) != 0 ||
          !voice_wav_cache_valid(path))
        {
          int rc;
          if (s_last_failure == DM_VOICE_FAILURE_NONE)
            {
              s_last_failure = DM_VOICE_FAILURE_TTS;
            }
          syslog(LOG_WARNING,
                 "[dm-voice] cloud volume sample unavailable; using tone\n");
          rc = dm_voice_beep();
          return rc == 0 ? 1 : -1;
        }
    }

  if (play_wav(path) == 0)
    {
      return 0;
    }
  return dm_voice_beep() == 0 ? 1 : -1;
}

int dm_voice_volume_test(void)
{
  int rc;

  pthread_mutex_lock(&s_voice_api_lock);
  rc = voice_volume_test_unlocked();
  pthread_mutex_unlock(&s_voice_api_lock);
  return rc;
}

/* Play local alert files away from the LVGL timer thread. */
struct voice_file_job_s
{
  char path[128];
};

static void *play_file_worker(void *arg)
{
  struct voice_file_job_s *job = arg;
  char path[sizeof(job->path)];

  snprintf(path, sizeof(path), "%s", job->path);
  free(job);
  (void)play_wav(path);
  return NULL;
}

int dm_voice_play_file(const char *wav_path)
{
  struct voice_file_job_s *job;
  pthread_t th;
  pthread_attr_t attr;
  int ret;

  if (!wav_path || !wav_path[0])
    {
      return -1;
    }

  job = malloc(sizeof(*job));
  if (!job)
    {
      return -1;
    }
  snprintf(job->path, sizeof(job->path), "%s", wav_path);

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 16384);
  ret = pthread_create(&th, &attr, play_file_worker, job);
  pthread_attr_destroy(&attr);
  if (ret != 0)
    {
      free(job);
      return -1;
    }

  pthread_detach(th);
  return 0;
}

#endif /* CONFIG_DESKMATE_APP */
