/*
 * dm_ui_recorder.c — standalone persistent voice recorder manager
 * All recorder, file-save and playback operations run outside the LVGL thread.
 */

#include "deskmate.h"
#include "dm_voice.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define RECORDER_MAX_SEC       30
#define RECORDER_SLOT_COUNT    10
#define RECORDER_STATE_PATH    "/data/flowdesk_recorder_state.txt"
#define RECORDER_STATE_TMP     "/data/flowdesk_recorder_state.tmp"
#define RECORDER_LEGACY_PATH   DM_VOICE_RECORD_PATH

typedef enum
{
  REC_JOB_NONE = 0,
  REC_JOB_START,
  REC_JOB_STOP_SAVE,
  REC_JOB_PLAY
} recorder_job_t;

static lv_obj_t *s_page;
static lv_obj_t *s_status;
static lv_obj_t *s_elapsed;
static lv_obj_t *s_record_btn;
static lv_obj_t *s_play_btn;
static lv_obj_t *s_list;
static lv_obj_t *s_rows[RECORDER_SLOT_COUNT];
static volatile int s_busy;
static int s_job_done;
static int s_job_result;
static int s_job_completed_slot;
static volatile recorder_job_t s_job;
static pthread_mutex_t s_job_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int s_recording;
static uint64_t s_record_started_ms;
static int s_slot_exists[RECORDER_SLOT_COUNT];
static int s_slot_count;
static int s_selected_slot = -1;
static int s_next_slot;
static int s_record_slot;
static int s_will_overwrite;

static uint64_t recorder_monotonic_ms(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
      return 0;
    }
  return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static int recorder_slot_path(int slot, char *path, size_t path_size)
{
  int n;

  if (!path || path_size == 0 || slot < 0 || slot >= RECORDER_SLOT_COUNT)
    {
      return -EINVAL;
    }

  n = snprintf(path, path_size, "/data/flowdesk_recording_%02d.wav", slot + 1);
  return n > 0 && n < (int)path_size ? 0 : -ENAMETOOLONG;
}

static int recorder_wav_exists(const char *path)
{
  FILE *f;
  unsigned char hdr[12];
  int valid = 0;

  if (!path)
    {
      return 0;
    }

  f = fopen(path, "rb");
  if (!f)
    {
      return 0;
    }
  if (fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr) &&
      memcmp(hdr, "RIFF", 4) == 0 && memcmp(hdr + 8, "WAVE", 4) == 0)
    {
      valid = 1;
    }
  fclose(f);
  return valid;
}

static void recorder_copy_legacy_if_needed(void)
{
  char dst[64];
  FILE *in;
  FILE *out;
  char buf[1024];
  size_t n;
  int i;
  int write_failed = 0;

  for (i = 0; i < RECORDER_SLOT_COUNT; i++)
    {
      if (s_slot_exists[i])
        {
          return;
        }
    }

  if (!recorder_wav_exists(RECORDER_LEGACY_PATH) ||
      recorder_slot_path(0, dst, sizeof(dst)) != 0)
    {
      return;
    }

  in = fopen(RECORDER_LEGACY_PATH, "rb");
  out = fopen(dst, "wb");
  if (!in || !out)
    {
      if (in)
        {
          fclose(in);
        }
      if (out)
        {
          fclose(out);
          unlink(dst);
        }
      return;
    }

  while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
    {
      if (fwrite(buf, 1, n, out) != n)
        {
          write_failed = 1;
          break;
        }
    }
  if (ferror(in))
    {
      write_failed = 1;
    }
  if (fclose(in) != 0)
    {
      write_failed = 1;
    }
  if (fclose(out) != 0)
    {
      write_failed = 1;
    }

  if (write_failed || !recorder_wav_exists(dst))
    {
      unlink(dst);
      return;
    }

  s_slot_exists[0] = 1;
  s_slot_count = 1;
}

static void recorder_state_save(int next_slot)
{
  FILE *f;

  if (next_slot < 0 || next_slot >= RECORDER_SLOT_COUNT)
    {
      return;
    }

  f = fopen(RECORDER_STATE_TMP, "w");
  if (!f)
    {
      return;
    }
  if (fprintf(f, "%d\n", next_slot) < 0)
    {
      fclose(f);
      unlink(RECORDER_STATE_TMP);
      return;
    }
  if (fclose(f) != 0)
    {
      unlink(RECORDER_STATE_TMP);
      return;
    }
  if (rename(RECORDER_STATE_TMP, RECORDER_STATE_PATH) != 0)
    {
      unlink(RECORDER_STATE_TMP);
    }
}

static void recorder_scan_slots(void)
{
  FILE *f;
  int next_slot = -1;
  int i;
  char path[64];

  s_slot_count = 0;
  s_selected_slot = -1;
  s_next_slot = 0;

  for (i = 0; i < RECORDER_SLOT_COUNT; i++)
    {
      if (recorder_slot_path(i, path, sizeof(path)) == 0 &&
          recorder_wav_exists(path))
        {
          s_slot_exists[i] = 1;
          s_slot_count++;
        }
      else
        {
          s_slot_exists[i] = 0;
        }
    }

  /* Keep recordings made by the previous single-file recorder visible. */
  recorder_copy_legacy_if_needed();

  f = fopen(RECORDER_STATE_PATH, "r");
  if (f)
    {
      if (fscanf(f, "%d", &next_slot) != 1 ||
          next_slot < 0 || next_slot >= RECORDER_SLOT_COUNT)
        {
          next_slot = -1;
        }
      fclose(f);
    }

  if (next_slot >= 0)
    {
      int previous = (next_slot + RECORDER_SLOT_COUNT - 1) %
                     RECORDER_SLOT_COUNT;
      s_next_slot = next_slot;
      if (s_slot_exists[previous])
        {
          s_selected_slot = previous;
        }
    }

  if (s_selected_slot < 0 && s_slot_count > 0)
    {
      for (i = 0; i < RECORDER_SLOT_COUNT; i++)
        {
          if (s_slot_exists[i])
            {
              s_selected_slot = i;
              s_next_slot = (i + 1) % RECORDER_SLOT_COUNT;
              break;
            }
        }
    }

  if (s_slot_count == 0)
    {
      s_next_slot = 0;
    }
}

static void recorder_refresh_list(void)
{
  int i;

  for (i = 0; i < RECORDER_SLOT_COUNT; i++)
    {
      char text[48];
      lv_obj_t *label;

      if (!s_rows[i])
        {
          continue;
        }

      if (s_slot_exists[i])
        {
          snprintf(text, sizeof(text),
                   i == s_selected_slot
                     ? dm_t("▶ 录音 %02d · 已选择", "▶ Recording %02d · selected")
                     : dm_t("录音 %02d · 可播放", "Recording %02d · ready"),
                   i + 1);
        }
      else
        {
          snprintf(text, sizeof(text), dm_t("录音 %02d · 空", "Recording %02d · empty"),
                   i + 1);
        }

      label = lv_obj_get_child(s_rows[i], 0);
      if (label)
        {
          lv_label_set_text(label, text);
        }
      lv_obj_set_style_bg_color(
          s_rows[i],
          (i == s_selected_slot) ? lv_color_hex(MD_PRIMARY_CONTAINER)
                                 : lv_color_hex(MD_SURFACE_CONTAINER),
          LV_PART_MAIN);
    }
}

static void recorder_refresh(void)
{
  const char *record_text;
  char play_text[40];
  lv_obj_t *label;

  if (!s_record_btn || !s_play_btn)
    {
      return;
    }

  if (s_busy)
    {
      record_text = (s_job == REC_JOB_START)
                      ? dm_t("启动中…", "Starting…")
                      : (s_job == REC_JOB_STOP_SAVE)
                          ? dm_t("正在保存…", "Saving…")
                          : dm_t("录音机忙…", "Recorder busy…");
    }
  else if (s_recording)
    {
      record_text = dm_t("停止并保存", "Stop & save");
    }
  else
    {
      record_text = dm_t("开始录音", "Start recording");
    }

  label = lv_obj_get_child(s_record_btn, 0);
  if (label)
    {
      lv_label_set_text(label, record_text);
    }

  if (s_selected_slot >= 0 && s_slot_exists[s_selected_slot])
    {
      snprintf(play_text, sizeof(play_text),
               dm_t("播放第 %02d 条", "Play #%02d"), s_selected_slot + 1);
    }
  else
    {
      snprintf(play_text, sizeof(play_text), dm_t("播放所选录音", "Play selected"));
    }
  label = lv_obj_get_child(s_play_btn, 0);
  if (label)
    {
      lv_label_set_text(label, play_text);
    }

  if (s_busy || s_recording || s_selected_slot < 0 ||
      !s_slot_exists[s_selected_slot])
    {
      lv_obj_add_state(s_play_btn, LV_STATE_DISABLED);
    }
  else
    {
      lv_obj_clear_state(s_play_btn, LV_STATE_DISABLED);
    }

  if (s_elapsed && !s_recording)
    {
      char info[64];
      if (s_selected_slot >= 0 && s_slot_exists[s_selected_slot])
        {
          snprintf(info, sizeof(info),
                   dm_t("录音 %d/10 · 已选 %02d", "%d/10 recordings · #%02d"),
                   s_slot_count, s_selected_slot + 1);
        }
      else
        {
          snprintf(info, sizeof(info),
                   dm_t("录音 %d/10 · 选择一条播放", "%d/10 recordings · select one"),
                   s_slot_count);
        }
      lv_label_set_text(s_elapsed, info);
    }

  lv_obj_set_style_bg_color(s_record_btn,
                            lv_color_hex(s_recording ? C_HEART : MD_PRIMARY),
                            LV_PART_MAIN);
  recorder_refresh_list();
}

static void *recorder_worker(void *arg)
{
  recorder_job_t job = s_job;
  int slot = (int)(intptr_t)arg;
  char path[64];
  int rc;

  switch (job)
    {
      case REC_JOB_START:
        rc = dm_voice_rec_start(RECORDER_MAX_SEC);
        break;

      case REC_JOB_STOP_SAVE:
        rc = dm_voice_rec_stop();
        if (rc == 0 && recorder_slot_path(slot, path, sizeof(path)) == 0)
          {
            rc = dm_voice_record_save(path);
            if (rc == 0)
              {
                recorder_state_save((slot + 1) % RECORDER_SLOT_COUNT);
              }
          }
        else if (rc == 0)
          {
            rc = -EINVAL;
          }
        break;

      case REC_JOB_PLAY:
        rc = recorder_slot_path(slot, path, sizeof(path));
        if (rc == 0)
          {
            rc = dm_voice_play_file_sync(path);
          }
        break;

      default:
        rc = -EINVAL;
        break;
    }

  pthread_mutex_lock(&s_job_lock);
  s_job_result = rc;
  s_job_completed_slot = slot;
  s_job_done = 1;
  pthread_mutex_unlock(&s_job_lock);
  return NULL;
}

static int recorder_start_job(recorder_job_t job, int slot)
{
  pthread_t th;
  pthread_attr_t attr;
  int ret;

  if (s_busy)
    {
      return -EBUSY;
    }

  s_busy = 1;
  s_job = job;
  pthread_mutex_lock(&s_job_lock);
  s_job_done = 0;
  pthread_mutex_unlock(&s_job_lock);
  recorder_refresh();

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 16384);
  ret = pthread_create(&th, &attr, recorder_worker, (void *)(intptr_t)slot);
  pthread_attr_destroy(&attr);
  if (ret != 0)
    {
      s_busy = 0;
      s_job = REC_JOB_NONE;
      if (s_status)
        {
          lv_label_set_text(s_status,
                            dm_t("无法启动录音任务", "Could not start recorder task"));
        }
      recorder_refresh();
      return -ret;
    }

  pthread_detach(th);
  return 0;
}

static void select_recording_cb(lv_event_t *e)
{
  int slot = (int)(intptr_t)lv_event_get_user_data(e);

  if (s_busy || s_recording || slot < 0 || slot >= RECORDER_SLOT_COUNT)
    {
      return;
    }
  s_selected_slot = slot;
  if (s_status)
    {
      if (s_slot_exists[slot])
        {
          char text[48];
          snprintf(text, sizeof(text), dm_t("已选择第 %02d 条录音", "Selected recording #%02d"),
                   slot + 1);
          lv_label_set_text(s_status, text);
        }
      else
        {
          lv_label_set_text(s_status, dm_t("此位置还没有录音", "This slot is empty"));
        }
    }
  recorder_refresh();
}

static void record_button_cb(lv_event_t *e)
{
  (void)e;

  if (s_busy)
    {
      return;
    }
  if (s_recording)
    {
      if (s_status)
        {
          lv_label_set_text(s_status,
                            dm_t("正在停止并保存…", "Stopping and saving…"));
        }
      (void)recorder_start_job(REC_JOB_STOP_SAVE, s_record_slot);
    }
  else
    {
      s_record_slot = s_next_slot;
      s_will_overwrite = s_slot_exists[s_record_slot];
      if (s_status)
        {
          if (s_will_overwrite)
            {
              lv_label_set_text(s_status,
                                dm_t("录音已满；保存后覆盖最旧槽位", "Full: oldest slot will be replaced"));
            }
          else
            {
              lv_label_set_text(s_status,
                                dm_t("正在启动录音…最多30秒", "Starting… up to 30 seconds"));
            }
        }
      (void)recorder_start_job(REC_JOB_START, s_record_slot);
    }
}

static void play_button_cb(lv_event_t *e)
{
  (void)e;

  if (s_busy || s_recording || s_selected_slot < 0 ||
      !s_slot_exists[s_selected_slot])
    {
      return;
    }
  if (s_status)
    {
      lv_label_set_text(s_status,
                        dm_t("正在通过喇叭播放…", "Playing through speaker…"));
    }
  (void)recorder_start_job(REC_JOB_PLAY, s_selected_slot);
}

static void back_button_cb(lv_event_t *e)
{
  (void)e;
  if (s_busy || s_recording)
    {
      if (s_status)
        {
          lv_label_set_text(s_status,
                            dm_t("请先停止并保存录音", "Stop and save first"));
        }
      return;
    }
  dm_show(PAGE_FEATURES);
}

void dm_create_recorder(void)
{
  lv_obj_t *title;
  lv_obj_t *back;
  int i;

  recorder_scan_slots();

  s_page = lv_obj_create(g_dm_root);
  lv_obj_set_size(s_page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(s_page, 0, 0);
  lv_obj_set_style_bg_color(s_page, lv_color_hex(MD_SURFACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_RECORDER] = s_page;

  back = dm_btn(s_page, "←", "<", 36, 26, MD_SURFACE_HIGH, MD_ON_SURFACE,
                back_button_cb, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_page, "录音管理", "Recordings", g_dm_font_m, MD_ON_SURFACE);
  lv_obj_set_pos(title, 54, 8);

  s_status = dm_lbl(s_page,
                    "最多10条 · 每条30秒 · 满后覆盖最旧",
                    "10 max · 30 sec each · oldest replaced",
                    g_dm_font_s, MD_ON_SURFACE_VARIANT);
  lv_obj_set_width(s_status, 296);
  lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_pos(s_status, 12, 34);

  s_elapsed = dm_lbl(s_page, "选择录音后可播放", "Select a recording to play",
                     g_dm_font_s, MD_ON_SURFACE_VARIANT);
  lv_obj_set_width(s_elapsed, 296);
  lv_obj_set_style_text_align(s_elapsed, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_pos(s_elapsed, 12, 52);

  s_list = lv_obj_create(s_page);
  lv_obj_set_size(s_list, 304, 84);
  lv_obj_set_pos(s_list, 8, 70);
  lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_list, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_row(s_list, 2, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);

  for (i = 0; i < RECORDER_SLOT_COUNT; i++)
    {
      lv_obj_t *row = lv_obj_create(s_list);
      lv_obj_set_size(row, 292, 24);
      lv_obj_set_style_bg_color(row, lv_color_hex(MD_SURFACE_CONTAINER),
                                LV_PART_MAIN);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_radius(row, 6, LV_PART_MAIN);
      lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_left(row, 8, LV_PART_MAIN);
      lv_obj_set_style_pad_right(row, 4, LV_PART_MAIN);
      lv_obj_set_style_pad_top(row, 1, LV_PART_MAIN);
      lv_obj_set_style_pad_bottom(row, 1, LV_PART_MAIN);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(row, select_recording_cb, LV_EVENT_CLICKED,
                          (void *)(intptr_t)i);
      (void)dm_lbl(row, "", "", g_dm_font_s, MD_ON_SURFACE);
      s_rows[i] = row;
    }

  s_record_btn = dm_btn(s_page, "开始录音", "Start recording", 286, 32,
                        MD_PRIMARY, MD_ON_PRIMARY,
                        record_button_cb, NULL);
  lv_obj_set_pos(s_record_btn, 17, 158);

  s_play_btn = dm_btn(s_page, "播放所选录音", "Play selected", 286, 32,
                      MD_SECONDARY_CONTAINER, MD_ON_SECONDARY_CONT,
                      play_button_cb, NULL);
  lv_obj_set_pos(s_play_btn, 17, 198);

  s_busy = 0;
  s_job_done = 0;
  s_job_result = 0;
  s_job_completed_slot = -1;
  s_job = REC_JOB_NONE;
  s_recording = 0;
  s_record_started_ms = 0;
  s_record_slot = s_next_slot;
  s_will_overwrite = 0;
  recorder_refresh();
}

void dm_recorder_tick(void)
{
  int job_done;
  int rc = 0;
  int completed_slot = -1;
  recorder_job_t completed = REC_JOB_NONE;

  pthread_mutex_lock(&s_job_lock);
  job_done = s_job_done;
  if (job_done)
    {
      completed = s_job;
      rc = s_job_result;
      completed_slot = s_job_completed_slot;
      s_job_done = 0;
    }
  pthread_mutex_unlock(&s_job_lock);

  if (job_done)
    {
      s_busy = 0;
      s_job = REC_JOB_NONE;

      if (completed == REC_JOB_START)
        {
          if (rc == 0)
            {
              s_recording = 1;
              s_record_started_ms = recorder_monotonic_ms();
              if (s_elapsed)
                {
                  char elapsed[48];
                  snprintf(elapsed, sizeof(elapsed),
                           dm_t("已录 0 / %u 秒", "0 / %u sec"),
                           RECORDER_MAX_SEC);
                  lv_label_set_text(s_elapsed, elapsed);
                }
              if (s_status)
                {
                  char status[48];
                  snprintf(status, sizeof(status),
                           dm_t("正在录制第 %02d 条…", "Recording #%02d…"),
                           s_record_slot + 1);
                  lv_label_set_text(s_status, status);
                }
            }
          else if (s_status)
            {
              lv_label_set_text(s_status,
                                dm_t("无法启动录音，请稍后重试",
                                     "Could not start recording"));
            }
        }
      else if (completed == REC_JOB_STOP_SAVE)
        {
          s_recording = 0;
          s_record_started_ms = 0;
          if (rc == 0 && completed_slot >= 0 &&
              completed_slot < RECORDER_SLOT_COUNT)
            {
              if (!s_slot_exists[completed_slot])
                {
                  s_slot_count++;
                }
              s_slot_exists[completed_slot] = 1;
              s_selected_slot = completed_slot;
              s_next_slot = (completed_slot + 1) % RECORDER_SLOT_COUNT;
              if (s_status)
                {
                  char status[64];
                  if (s_will_overwrite)
                    {
                      snprintf(status, sizeof(status),
                               dm_t("已保存第 %02d 条，覆盖循环槽位",
                                    "Saved #%02d; oldest slot replaced"),
                               completed_slot + 1);
                    }
                  else
                    {
                      snprintf(status, sizeof(status),
                               dm_t("第 %02d 条录音已保存",
                                    "Recording #%02d saved"),
                               completed_slot + 1);
                    }
                  lv_label_set_text(s_status, status);
                }
            }
          else if (s_status)
            {
              lv_label_set_text(s_status,
                                dm_t("录音保存失败，请重新录制",
                                     "Save failed; please record again"));
            }
          s_will_overwrite = 0;
        }
      else if (completed == REC_JOB_PLAY && s_status)
        {
          lv_label_set_text(s_status,
                            rc == 0
                              ? dm_t("录音播放完成", "Playback complete")
                              : dm_t("播放失败，请检查喇叭", "Playback failed; check speaker"));
        }
      recorder_refresh();
    }

  if (s_recording && !s_busy)
    {
      uint64_t now_ms = recorder_monotonic_ms();
      if (s_record_started_ms != 0 && now_ms >= s_record_started_ms)
        {
          char elapsed[48];
          unsigned int seconds =
              (unsigned int)((now_ms - s_record_started_ms) / 1000);
          if (seconds > RECORDER_MAX_SEC)
            {
              seconds = RECORDER_MAX_SEC;
            }
          snprintf(elapsed, sizeof(elapsed),
                   dm_t("已录 %u / %u 秒", "%u / %u sec"),
                   seconds, RECORDER_MAX_SEC);
          if (s_elapsed)
            {
              lv_label_set_text(s_elapsed, elapsed);
            }
        }

      /* nxrecorder stops itself at the configured time limit. Save its
       * completed capture instead of leaving it in /tmp. */
      if (!dm_voice_rec_active())
        {
          if (recorder_start_job(REC_JOB_STOP_SAVE, s_record_slot) != 0 && s_status)
            {
              lv_label_set_text(s_status,
                                dm_t("自动结束，正在等待保存", "Stopped; retrying save"));
            }
        }
    }
}

#endif /* CONFIG_DESKMATE_APP */
