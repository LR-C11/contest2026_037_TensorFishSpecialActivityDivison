/****************************************************************************
 * dm_ui_meeting.c — local meeting capture and AI brief workflow
 * All recorder/network/TTS work runs on a worker, never the LVGL thread.
 ****************************************************************************/

#include "deskmate.h"
#include "dm_voice.h"

#ifdef CONFIG_DESKMATE_APP

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MEETING_SLOTS 3
#define MEETING_MAX_SEC 600
#define MEETING_STATE_PATH "/data/flowdesk_meeting_state.txt"
#define MEETING_STATE_TMP  "/data/flowdesk_meeting_state.tmp"
#define MEETING_TRANSCRIPT_CAP DM_VOICE_MEETING_TRANSCRIPT_MAX
#define MEETING_SUMMARY_CAP DM_VOICE_MEETING_SUMMARY_MAX

typedef enum
{
  MEETING_JOB_NONE = 0,
  MEETING_JOB_START,
  MEETING_JOB_STOP_SAVE,
  MEETING_JOB_ANALYZE,
  MEETING_JOB_PLAY
} meeting_job_t;

static lv_obj_t *s_page;
static lv_obj_t *s_detail_page;
static lv_obj_t *s_status;
static lv_obj_t *s_elapsed;
static lv_obj_t *s_progress_fill;
static lv_obj_t *s_record_btn;
static lv_obj_t *s_analyze_btn;
static lv_obj_t *s_rows[MEETING_SLOTS];
static lv_obj_t *s_detail_title;
static lv_obj_t *s_summary_label;
static lv_obj_t *s_transcript_label;
static volatile int s_busy;
static volatile int s_recording;
static volatile meeting_job_t s_job;
static int s_job_done;
static int s_job_result;
static int s_job_store_result;
static int s_job_completed_slot;
static pthread_mutex_t s_job_lock = PTHREAD_MUTEX_INITIALIZER;
static int s_audio_exists[MEETING_SLOTS];
static int s_summary_exists[MEETING_SLOTS];
static int s_count;
static int s_next_slot;
static int s_record_slot;
static int s_selected_slot = -1;
static int s_will_overwrite;
static uint64_t s_record_started_ms;
static int s_elapsed_sec;
static int s_detail_slot = -1;
static int s_memory_summary_slot = -1;
static char s_result_transcript[MEETING_TRANSCRIPT_CAP];
static char s_result_summary[MEETING_SUMMARY_CAP];
static char s_detail_transcript[MEETING_TRANSCRIPT_CAP];
static char s_detail_summary[MEETING_SUMMARY_CAP];

static void *meeting_worker(void *arg);
static uint64_t meeting_monotonic_ms(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
      return 0;
    }
  return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static int meeting_paths(int slot, char *audio, size_t audio_cap,
                         char *meta, size_t meta_cap)
{
  int a;
  int m;

  if (slot < 0 || slot >= MEETING_SLOTS || !audio || !meta)
    {
      return -EINVAL;
    }
  a = snprintf(audio, audio_cap, "/data/flowdesk_meeting_%02d.wav", slot + 1);
  m = snprintf(meta, meta_cap, "/data/flowdesk_meeting_%02d.meta", slot + 1);
  return a > 0 && a < (int)audio_cap && m > 0 && m < (int)meta_cap
           ? 0 : -ENAMETOOLONG;
}

static int meeting_audio_valid(const char *path)
{
  unsigned char header[12];
  FILE *f;
  int valid;

  f = fopen(path, "rb");
  if (!f)
    {
      return 0;
    }
  valid = fread(header, 1, sizeof(header), f) == sizeof(header) &&
          memcmp(header, "RIFF", 4) == 0 &&
          memcmp(header + 8, "WAVE", 4) == 0;
  fclose(f);
  return valid;
}

static int meeting_info_header(int slot, long *audio_size, long *audio_mtime,
                               unsigned long *transcript_len,
                               unsigned long *summary_len)
{
  char audio[80];
  char meta[80];
  char line[128];
  struct stat st;
  FILE *f;
  int matched;

  if (meeting_paths(slot, audio, sizeof(audio), meta, sizeof(meta)) != 0 ||
      stat(audio, &st) != 0)
    {
      return 0;
    }
  f = fopen(meta, "rb");
  if (!f)
    {
      return 0;
    }
  matched = fgets(line, sizeof(line), f) != NULL &&
            sscanf(line, "FMB1 %ld %ld %lu %lu", audio_size, audio_mtime,
                   transcript_len, summary_len) == 4 &&
            *audio_size == (long)st.st_size &&
            *audio_mtime == (long)st.st_mtime &&
            *transcript_len < MEETING_TRANSCRIPT_CAP &&
            *summary_len < MEETING_SUMMARY_CAP;
  fclose(f);
  return matched;
}

static int meeting_info_load(int slot, char *transcript, size_t transcript_cap,
                             char *summary, size_t summary_cap)
{
  char audio[80];
  char meta[80];
  long audio_size;
  long audio_mtime;
  unsigned long transcript_len;
  unsigned long summary_len;
  FILE *f;
  size_t got_transcript;
  size_t got_summary;

  if (!transcript || transcript_cap < 2 || !summary || summary_cap < 2 ||
      !meeting_info_header(slot, &audio_size, &audio_mtime, &transcript_len,
                           &summary_len) ||
      meeting_paths(slot, audio, sizeof(audio), meta, sizeof(meta)) != 0)
    {
      return -1;
    }

  f = fopen(meta, "rb");
  if (!f)
    {
      return -1;
    }
  {
    char line[128];
    if (!fgets(line, sizeof(line), f))
      {
        fclose(f);
        return -1;
      }
  }
  got_transcript = fread(transcript, 1, transcript_len, f);
  got_summary = fread(summary, 1, summary_len, f);
  fclose(f);
  if (got_transcript != transcript_len || got_summary != summary_len ||
      transcript_len >= transcript_cap || summary_len >= summary_cap)
    {
      transcript[0] = '\0';
      summary[0] = '\0';
      return -1;
    }
  transcript[transcript_len] = '\0';
  summary[summary_len] = '\0';
  return 0;
}

static int meeting_info_save(int slot, const char *transcript,
                             const char *summary)
{
  char audio[80];
  char meta[80];
  char temp[96];
  struct stat st;
  FILE *f;
  size_t transcript_len;
  size_t summary_len;
  int n;
  int failed = 0;

  if (!transcript || !summary ||
      meeting_paths(slot, audio, sizeof(audio), meta, sizeof(meta)) != 0 ||
      stat(audio, &st) != 0)
    {
      return -1;
    }
  transcript_len = strlen(transcript);
  summary_len = strlen(summary);
  if (transcript_len >= MEETING_TRANSCRIPT_CAP ||
      summary_len >= MEETING_SUMMARY_CAP)
    {
      return -1;
    }
  n = snprintf(temp, sizeof(temp), "%s.tmp", meta);
  if (n <= 0 || n >= (int)sizeof(temp))
    {
      return -1;
    }
  f = fopen(temp, "wb");
  if (!f)
    {
      return -1;
    }
  if (fprintf(f, "FMB1 %ld %ld %lu %lu\n", (long)st.st_size,
              (long)st.st_mtime, (unsigned long)transcript_len,
              (unsigned long)summary_len) < 0 ||
      fwrite(transcript, 1, transcript_len, f) != transcript_len ||
      fwrite(summary, 1, summary_len, f) != summary_len)
    {
      failed = 1;
    }
  if (fclose(f) != 0)
    {
      failed = 1;
    }
  if (failed || rename(temp, meta) != 0)
    {
      unlink(temp);
      return -1;
    }
  return 0;
}

static int meeting_state_save(int next_slot)
{
  FILE *f;
  if (next_slot < 0 || next_slot >= MEETING_SLOTS)
    {
      return -EINVAL;
    }
  f = fopen(MEETING_STATE_TMP, "w");
  if (!f)
    {
      return -errno;
    }
  {
    int write_failed = fprintf(f, "%d\n", next_slot) < 0;
    if (fclose(f) != 0)
      {
        write_failed = 1;
      }
    if (write_failed)
      {
        unlink(MEETING_STATE_TMP);
        return -EIO;
      }
  }
  if (rename(MEETING_STATE_TMP, MEETING_STATE_PATH) != 0)
    {
      int saved_errno = errno;
      unlink(MEETING_STATE_TMP);
      return -saved_errno;
    }
  return 0;
}

static void meeting_scan(void)
{
  char audio[80];
  char meta[80];
  FILE *f;
  int next = -1;
  int oldest_slot = -1;
  int newest_slot = -1;
  time_t oldest_mtime = 0;
  time_t newest_mtime = 0;
  int i;

  s_count = 0;
  s_selected_slot = -1;
  for (i = 0; i < MEETING_SLOTS; i++)
    {
      long audio_size;
      long audio_mtime;
      unsigned long transcript_len;
      unsigned long summary_len;
      struct stat st;
      s_audio_exists[i] = meeting_paths(i, audio, sizeof(audio), meta,
                                        sizeof(meta)) == 0 &&
                          meeting_audio_valid(audio) && stat(audio, &st) == 0;
      s_summary_exists[i] = s_audio_exists[i] &&
          meeting_info_header(i, &audio_size, &audio_mtime,
                              &transcript_len, &summary_len);
      if (s_audio_exists[i])
        {
          s_count++;
          if (newest_slot < 0 || st.st_mtime >= newest_mtime)
            {
              newest_slot = i;
              newest_mtime = st.st_mtime;
            }
          if (oldest_slot < 0 || st.st_mtime < oldest_mtime)
            {
              oldest_slot = i;
              oldest_mtime = st.st_mtime;
            }
        }
    }
  s_selected_slot = newest_slot;

  f = fopen(MEETING_STATE_PATH, "r");
  if (f)
    {
      if (fscanf(f, "%d", &next) != 1 || next < 0 || next >= MEETING_SLOTS)
        {
          next = -1;
        }
      fclose(f);
    }
  if (next >= 0)
    {
      s_next_slot = next;
      if (s_audio_exists[(next + MEETING_SLOTS - 1) % MEETING_SLOTS])
        {
          s_selected_slot = (next + MEETING_SLOTS - 1) % MEETING_SLOTS;
        }
    }
  else
    {
      s_next_slot = oldest_slot >= 0 ? oldest_slot : 0;
      for (i = 0; i < MEETING_SLOTS; i++)
        {
          if (!s_audio_exists[i])
            {
              s_next_slot = i;
              break;
            }
        }
    }
}

static void meeting_refresh_list(void)
{
  int i;
  for (i = 0; i < MEETING_SLOTS; i++)
    {
      char text[56];
      lv_obj_t *label;
      if (!s_rows[i]) continue;
      if (!s_audio_exists[i])
        {
          snprintf(text, sizeof(text), "会议 %d · 空", i + 1);
        }
      else if (s_summary_exists[i])
        {
          snprintf(text, sizeof(text),
                   i == s_selected_slot ? "会议 %d · 摘要已生成 · 已选" :
                                          "会议 %d · 摘要已生成",
                   i + 1);
        }
      else
        {
          snprintf(text, sizeof(text),
                   i == s_selected_slot ? "会议 %d · 已录音 · 待分析 · 已选" :
                                          "会议 %d · 已录音 · 待分析",
                   i + 1);
        }
      label = lv_obj_get_child(s_rows[i], 0);
      if (label) lv_label_set_text(label, text);
      lv_obj_set_style_bg_color(s_rows[i],
          lv_color_hex(i == s_selected_slot ? MD_PRIMARY_CONTAINER :
                                                MD_SURFACE_CONTAINER),
          LV_PART_MAIN);
    }
}

static void meeting_refresh(void)
{
  int completed;
  int total;
  char status[112];
  char elapsed[40];
  int width;
  lv_obj_t *button_label;

  if (s_record_btn)
    {
      button_label = lv_obj_get_child(s_record_btn, 0);
      if (button_label)
        {
          lv_label_set_text(button_label,
                            s_recording ? dm_t("■ 结束并保存", "■ Stop & save") :
                                          dm_t("● 开始录音", "● Record"));
        }
      lv_obj_set_style_bg_color(s_record_btn,
          lv_color_hex(s_recording ? C_HEART : MD_PRIMARY), LV_PART_MAIN);
    }
  if (s_elapsed)
    {
      snprintf(elapsed, sizeof(elapsed), "%02d:%02d / 10:00",
               s_elapsed_sec / 60, s_elapsed_sec % 60);
      lv_label_set_text(s_elapsed, elapsed);
    }
  if (s_progress_fill)
    {
      width = 280 * s_elapsed_sec / MEETING_MAX_SEC;
      if (s_recording && width < 2) width = 2;
      lv_obj_set_width(s_progress_fill, width);
    }
  if (s_analyze_btn)
    {
      button_label = lv_obj_get_child(s_analyze_btn, 0);
      if (button_label)
        {
          if (s_busy && s_job == MEETING_JOB_ANALYZE)
            {
              dm_voice_meeting_progress(&completed, &total);
              if (total > 0 && completed >= total)
                {
                  snprintf(status, sizeof(status), "正在保存转写和摘要…");
                }
              else if (total > 0)
                {
                  snprintf(status, sizeof(status), "分析中 %d/%d", completed,
                           total);
                }
              else
                {
                  snprintf(status, sizeof(status), "正在准备分析…");
                }
              lv_label_set_text(button_label, status);
            }
          else if (s_selected_slot >= 0 &&
                   s_summary_exists[s_selected_slot])
            {
              lv_label_set_text(button_label, dm_t("查看摘要", "View brief"));
            }
          else
            {
              lv_label_set_text(button_label, dm_t("开始分析", "Analyze"));
            }
        }
      if (s_busy || s_recording || s_selected_slot < 0 ||
          !s_audio_exists[s_selected_slot])
        lv_obj_add_state(s_analyze_btn, LV_STATE_DISABLED);
      else
        lv_obj_clear_state(s_analyze_btn, LV_STATE_DISABLED);
    }
  meeting_refresh_list();
}

static void meeting_detail_refresh(void)
{
  int slot = s_detail_slot;
  char title[48];

  if (slot < 0 || slot >= MEETING_SLOTS || !s_summary_exists[slot])
    {
      snprintf(title, sizeof(title), "会议 %02d · 尚无摘要", slot + 1);
      lv_label_set_text(s_summary_label, "该录音尚未分析。返回列表后点击‘开始分析’。\n");
      lv_label_set_text(s_transcript_label, "");
    }
  else
    {
      if (slot == s_memory_summary_slot)
        {
          snprintf(s_detail_transcript, sizeof(s_detail_transcript), "%s",
                   s_result_transcript);
          snprintf(s_detail_summary, sizeof(s_detail_summary), "%s",
                   s_result_summary);
        }
      else if (meeting_info_load(slot, s_detail_transcript,
                                 sizeof(s_detail_transcript), s_detail_summary,
                                 sizeof(s_detail_summary)) != 0)
        {
          s_detail_transcript[0] = '\0';
          s_detail_summary[0] = '\0';
        }
      snprintf(title, sizeof(title), "会议摘要 %02d", slot + 1);
      lv_label_set_text(s_summary_label, s_detail_summary[0]
                          ? s_detail_summary : "摘要文件读取失败");
      lv_label_set_text(s_transcript_label, s_detail_transcript[0]
                          ? s_detail_transcript : "转写文本不可用");
    }

  if (s_detail_title) lv_label_set_text(s_detail_title, title);
}

static void meeting_select_cb(lv_event_t *e)
{
  int slot = (int)(intptr_t)lv_event_get_user_data(e);
  if (s_busy || s_recording || slot < 0 || slot >= MEETING_SLOTS ||
      !s_audio_exists[slot])
    {
      return;
    }
  s_selected_slot = slot;
  meeting_refresh();
}

static int meeting_store_path(int slot, char *path, size_t cap)
{
  int n;
  if (slot < 0 || slot >= MEETING_SLOTS || !path)
    {
      return -EINVAL;
    }
  n = snprintf(path, cap, "/data/flowdesk_meeting_%02d.wav", slot + 1);
  return n > 0 && n < (int)cap ? 0 : -ENAMETOOLONG;
}

static int meeting_start_worker(meeting_job_t job, int slot)
{
  pthread_t thread;
  pthread_attr_t attr;
  int rc;

  if (s_busy) return -EBUSY;
  s_busy = 1;
  s_job = job;
  pthread_mutex_lock(&s_job_lock);
  s_job_done = 0;
  s_job_result = 0;
  s_job_store_result = 0;
  s_job_completed_slot = slot;
  pthread_mutex_unlock(&s_job_lock);
  meeting_refresh();

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, DM_VOICE_NET_WORKER_STACK);
  rc = pthread_create(&thread, &attr, meeting_worker, (void *)(intptr_t)slot);
  pthread_attr_destroy(&attr);
  if (rc != 0)
    {
      s_busy = 0;
      s_job = MEETING_JOB_NONE;
      meeting_refresh();
      return -rc;
    }
  pthread_detach(thread);
  return 0;
}

static void meeting_record_cb(lv_event_t *e)
{
  (void)e;
  if (s_busy) return;
  if (s_recording)
    {
      if (s_status) lv_label_set_text(s_status, "正在结束并保存会议录音…");
      if (meeting_start_worker(MEETING_JOB_STOP_SAVE, s_record_slot) != 0 &&
          s_status)
        lv_label_set_text(s_status, "停止任务启动失败");
      return;
    }

  s_record_slot = s_next_slot;
  s_will_overwrite = s_audio_exists[s_record_slot];
  s_elapsed_sec = 0;
  if (s_status)
    {
      lv_label_set_text(s_status, s_will_overwrite
          ? "空间已满：本条将替换最早的会议记录"
          : "录音将保存在本机 · 最长10分钟");
    }
  if (meeting_start_worker(MEETING_JOB_START, s_record_slot) != 0 && s_status)
    lv_label_set_text(s_status, "录音任务启动失败");
}

static void meeting_analyze_cb(lv_event_t *e)
{
  (void)e;
  if (s_busy || s_recording || s_selected_slot < 0 ||
      !s_audio_exists[s_selected_slot])
    {
      return;
    }
  if (s_summary_exists[s_selected_slot])
    {
      s_detail_slot = s_selected_slot;
      meeting_detail_refresh();
      dm_show(PAGE_MEETING_DETAIL);
      return;
    }
  if (s_status) lv_label_set_text(s_status, "正在分段识别并提炼会议重点…");
  if (meeting_start_worker(MEETING_JOB_ANALYZE, s_selected_slot) != 0 && s_status)
    lv_label_set_text(s_status, "分析任务启动失败");
}

static void meeting_row_cb(lv_event_t *e)
{
  int slot = (int)(intptr_t)lv_event_get_user_data(e);
  if (s_busy || s_recording || slot < 0 || slot >= MEETING_SLOTS ||
      !s_audio_exists[slot])
    {
      return;
    }
  s_selected_slot = slot;
  meeting_refresh();
  if (s_summary_exists[slot])
    {
      s_detail_slot = slot;
      meeting_detail_refresh();
      dm_show(PAGE_MEETING_DETAIL);
    }
  else if (s_status)
    {
      lv_label_set_text(s_status, "已选择会议录音，可点击开始分析");
    }
}

static void meeting_back_cb(lv_event_t *e)
{
  (void)e;
  if (s_busy || s_recording)
    {
      if (s_status)
        {
          lv_label_set_text(s_status, "请先结束录音或等待当前任务完成");
        }
      return;
    }
  dm_show(PAGE_FEATURES);
}

static void meeting_detail_back_cb(lv_event_t *e)
{
  (void)e;
  if (s_busy)
    {
      return;
    }
  dm_show(PAGE_MEETING);
}

static void meeting_detail_play_cb(lv_event_t *e)
{
  (void)e;
  if (s_busy || s_detail_slot < 0 || s_detail_slot >= MEETING_SLOTS ||
      !s_audio_exists[s_detail_slot]) return;
  if (meeting_start_worker(MEETING_JOB_PLAY, s_detail_slot) != 0 && s_status)
    lv_label_set_text(s_status, "播放任务启动失败");
}

static void meeting_detail_create(void)
{
  lv_obj_t *back;
  lv_obj_t *play;
  lv_obj_t *sc;
  lv_obj_t *title;

  s_detail_page = lv_obj_create(g_dm_root);
  lv_obj_set_size(s_detail_page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(s_detail_page, 0, 0);
  lv_obj_set_style_bg_color(s_detail_page, lv_color_hex(MD_SURFACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_detail_page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_detail_page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_detail_page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_detail_page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_MEETING_DETAIL] = s_detail_page;

  back = dm_btn(s_detail_page, "←", "<", 36, 26, MD_SURFACE_HIGH,
                MD_ON_SURFACE, meeting_detail_back_cb, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_detail_page, "会议详情", "Meeting brief", g_dm_font_m,
                 MD_ON_SURFACE);
  lv_obj_set_pos(title, 54, 8);
  s_detail_title = title;
  play = dm_btn(s_detail_page, "▶ 录音", "▶ Audio", 78, 26,
                MD_SECONDARY_CONTAINER, MD_ON_SECONDARY_CONT,
                meeting_detail_play_cb, NULL);
  lv_obj_set_pos(play, 232, 6);

  sc = lv_obj_create(s_detail_page);
  lv_obj_set_size(sc, 304, 190);
  lv_obj_set_pos(sc, 8, 38);
  lv_obj_set_style_bg_color(sc, lv_color_hex(MD_SURFACE_CONTAINER), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sc, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(sc, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(sc, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_all(sc, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_row(sc, 8, LV_PART_MAIN);
  lv_obj_set_scroll_dir(sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(sc, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(sc, LV_FLEX_FLOW_COLUMN);

  s_summary_label = lv_label_create(sc);
  lv_label_set_text(s_summary_label, "会议摘要尚未生成");
  lv_obj_set_width(s_summary_label, 278);
  lv_label_set_long_mode(s_summary_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_summary_label, g_dm_font_s, LV_PART_MAIN);
  lv_obj_set_style_text_color(s_summary_label,
                              lv_color_hex(MD_ON_SURFACE), LV_PART_MAIN);
  s_transcript_label = lv_label_create(sc);
  lv_label_set_text(s_transcript_label, "");
  lv_obj_set_width(s_transcript_label, 278);
  lv_label_set_long_mode(s_transcript_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(s_transcript_label, g_dm_font_s, LV_PART_MAIN);
  lv_obj_set_style_text_color(s_transcript_label,
                              lv_color_hex(MD_ON_SURFACE_VARIANT), LV_PART_MAIN);
}

static void meeting_worker_finish(int rc, int store_rc, int slot)
{
  pthread_mutex_lock(&s_job_lock);
  s_job_result = rc;
  s_job_store_result = store_rc;
  s_job_completed_slot = slot;
  s_job_done = 1;
  pthread_mutex_unlock(&s_job_lock);
}

static void *meeting_worker(void *arg)
{
  meeting_job_t job = s_job;
  int slot = (int)(intptr_t)arg;
  char path[80];
  int rc = -EINVAL;
  int store_rc = 0;

  switch (job)
    {
      case MEETING_JOB_START:
        rc = dm_voice_rec_start(MEETING_MAX_SEC);
        break;

      case MEETING_JOB_STOP_SAVE:
        rc = dm_voice_rec_stop();
        if (rc == 0 && meeting_store_path(slot, path, sizeof(path)) == 0)
          {
            rc = dm_voice_record_save(path);
            if (rc == 0)
              {
                char meta[96];
                snprintf(meta, sizeof(meta), "/data/flowdesk_meeting_%02d.meta",
                         slot + 1);
                unlink(meta);
                store_rc = meeting_state_save((slot + 1) % MEETING_SLOTS);
              }
          }
        break;

      case MEETING_JOB_ANALYZE:
        rc = meeting_store_path(slot, path, sizeof(path));
        if (rc == 0)
          {
            rc = dm_voice_meeting_analyze(path, s_result_transcript,
                                          sizeof(s_result_transcript),
                                          s_result_summary,
                                          sizeof(s_result_summary));
            if (rc >= 0)
              {
                int api_rc = rc;
                store_rc = meeting_info_save(slot, s_result_transcript,
                                             s_result_summary);
                rc = api_rc;
              }
          }
        break;

      case MEETING_JOB_PLAY:
        rc = meeting_store_path(slot, path, sizeof(path));
        if (rc == 0) rc = dm_voice_play_file_sync(path);
        break;

      default:
        break;
    }

  meeting_worker_finish(rc, store_rc, slot);
  return NULL;
}

void dm_create_meeting(void)
{
  lv_obj_t *title;
  lv_obj_t *back;
  lv_obj_t *label;
  int i;

  mkdir("/data", 0755);
  meeting_scan();
  s_page = lv_obj_create(g_dm_root);
  lv_obj_set_size(s_page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(s_page, 0, 0);
  lv_obj_set_style_bg_color(s_page, lv_color_hex(MD_SURFACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_MEETING] = s_page;

  back = dm_btn(s_page, "←", "<", 36, 26, MD_SURFACE_HIGH, MD_ON_SURFACE,
                meeting_back_cb, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_page, "会议摘要", "Meeting brief", g_dm_font_m,
                 MD_ON_SURFACE);
  lv_obj_set_pos(title, 54, 8);

  s_status = dm_lbl(s_page, "录音保存在本机 · 最多3份", "Saved locally · 3 meetings max",
                    g_dm_font_s, MD_ON_SURFACE_VARIANT);
  lv_obj_set_width(s_status, 300);
  lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_pos(s_status, 10, 32);
  s_elapsed = dm_lbl(s_page, "00:00 / 10:00", "00:00 / 10:00",
                     g_dm_font_l, MD_ON_SURFACE);
  lv_obj_set_width(s_elapsed, 300);
  lv_obj_set_style_text_align(s_elapsed, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_pos(s_elapsed, 10, 49);

  {
    lv_obj_t *track = lv_obj_create(s_page);
    lv_obj_set_size(track, 280, 6);
    lv_obj_align(track, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_set_style_bg_color(track, lv_color_hex(0x3a414b), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(track, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(track, 3, LV_PART_MAIN);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    s_progress_fill = lv_obj_create(track);
    lv_obj_set_size(s_progress_fill, 0, 6);
    lv_obj_set_pos(s_progress_fill, 0, 0);
    lv_obj_set_style_bg_color(s_progress_fill, lv_color_hex(C_HEART), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_progress_fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_progress_fill, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_progress_fill, 3, LV_PART_MAIN);
    lv_obj_clear_flag(s_progress_fill, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  }

  s_record_btn = dm_btn(s_page, "● 开始录音", "● Record", 142, 46,
                        MD_PRIMARY, MD_ON_PRIMARY, meeting_record_cb, NULL);
  lv_obj_set_style_radius(s_record_btn, 23, LV_PART_MAIN);
  lv_obj_set_pos(s_record_btn, 14, 94);
  s_analyze_btn = dm_btn(s_page, "开始分析", "Analyze", 142, 46,
                         MD_SECONDARY_CONTAINER, MD_ON_SECONDARY_CONT,
                         meeting_analyze_cb, NULL);
  lv_obj_set_style_radius(s_analyze_btn, 16, LV_PART_MAIN);
  lv_obj_set_pos(s_analyze_btn, 164, 94);

  label = dm_lbl(s_page, "最近会议 · 最多保留3份", "Recent · keep 3",
                 g_dm_font_s, MD_ON_SURFACE_VARIANT);
  lv_obj_set_pos(label, 14, 146);
  for (i = 0; i < MEETING_SLOTS; i++)
    {
      s_rows[i] = lv_obj_create(s_page);
      lv_obj_set_size(s_rows[i], 292, 24);
      lv_obj_set_pos(s_rows[i], 14, 164 + i * 24);
      lv_obj_set_style_bg_color(s_rows[i], lv_color_hex(MD_SURFACE_CONTAINER),
                                LV_PART_MAIN);
      lv_obj_set_style_bg_opa(s_rows[i], LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_radius(s_rows[i], 8, LV_PART_MAIN);
      lv_obj_set_style_border_width(s_rows[i], 0, LV_PART_MAIN);
      lv_obj_set_style_pad_left(s_rows[i], 8, LV_PART_MAIN);
      lv_obj_clear_flag(s_rows[i], LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(s_rows[i], LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(s_rows[i], meeting_row_cb, LV_EVENT_CLICKED,
                          (void *)(intptr_t)i);
      (void)dm_lbl(s_rows[i], "", "", g_dm_font_s, MD_ON_SURFACE);
    }

  meeting_detail_create();
  s_busy = 0;
  s_recording = 0;
  s_job = MEETING_JOB_NONE;
  s_job_done = 0;
  s_record_started_ms = 0;
  s_elapsed_sec = 0;
  s_memory_summary_slot = -1;
  meeting_refresh();
}

void dm_meeting_tick(void)
{
  int done;
  int rc;
  int store_rc;
  int slot;
  meeting_job_t job;

  pthread_mutex_lock(&s_job_lock);
  done = s_job_done;
  if (done)
    {
      job = s_job;
      rc = s_job_result;
      store_rc = s_job_store_result;
      slot = s_job_completed_slot;
      s_job_done = 0;
    }
  else
    {
      job = MEETING_JOB_NONE;
      rc = 0;
      store_rc = 0;
      slot = -1;
    }
  pthread_mutex_unlock(&s_job_lock);

  if (done)
    {
      s_busy = 0;
      s_job = MEETING_JOB_NONE;
      if (job == MEETING_JOB_START)
        {
          if (rc == 0)
            {
              s_recording = 1;
              s_record_started_ms = meeting_monotonic_ms();
              s_elapsed_sec = 0;
              if (s_status) lv_label_set_text(s_status, "录音中 · 再点一次结束并保存");
            }
          else if (s_status)
            {
              lv_label_set_text(s_status, "录音启动失败，请检查麦克风");
            }
        }
      else if (job == MEETING_JOB_STOP_SAVE)
        {
          s_recording = 0;
          s_record_started_ms = 0;
          if (rc == 0 && slot >= 0 && slot < MEETING_SLOTS)
            {
              char meta[96];
              if (!s_audio_exists[slot]) s_count++;
              s_audio_exists[slot] = 1;
              s_summary_exists[slot] = 0;
              if (s_memory_summary_slot == slot)
                {
                  s_memory_summary_slot = -1;
                }
              s_selected_slot = slot;
              s_next_slot = (slot + 1) % MEETING_SLOTS;
              snprintf(meta, sizeof(meta), "/data/flowdesk_meeting_%02d.meta",
                       slot + 1);
              unlink(meta);
              if (s_status)
                lv_label_set_text(s_status, store_rc == 0
                    ? "录音已保存到本机，可点击开始分析"
                    : "录音已保存；索引保存失败");
            }
          else if (s_status)
            {
              lv_label_set_text(s_status, "录音保存失败，请重试");
            }
        }
      else if (job == MEETING_JOB_ANALYZE)
        {
          if (rc >= 0 && slot >= 0 && slot < MEETING_SLOTS)
            {
              s_memory_summary_slot = slot;
              s_summary_exists[slot] = 1;
              s_selected_slot = slot;
              if (store_rc == 0)
                {
                  if (s_status)
                    lv_label_set_text(s_status, rc == 0
                        ? "摘要已保存并播报"
                        : "摘要已保存；TTS 播报失败");
                }
              else if (s_status)
                {
                  lv_label_set_text(s_status, "摘要已生成但本地保存失败");
                }
              s_detail_slot = slot;
              meeting_detail_refresh();
              dm_show(PAGE_MEETING_DETAIL);
            }
          else if (s_status)
            {
              lv_label_set_text(s_status, "分析失败，请检查网络后重试");
            }
        }
      else if (job == MEETING_JOB_PLAY && s_status)
        {
          lv_label_set_text(s_status, rc == 0 ? "会议录音播放完成" : "会议录音播放失败");
        }
      meeting_refresh();
    }

  if (s_busy && s_job == MEETING_JOB_ANALYZE)
    {
      meeting_refresh();
    }

  if (s_recording && !s_busy)
    {
      uint64_t now = meeting_monotonic_ms();
      if (s_record_started_ms && now >= s_record_started_ms)
        {
          s_elapsed_sec = (int)((now - s_record_started_ms) / 1000);
          if (s_elapsed_sec > MEETING_MAX_SEC) s_elapsed_sec = MEETING_MAX_SEC;
          meeting_refresh();
        }
      if (!dm_voice_rec_active())
        {
          if (s_status) lv_label_set_text(s_status, "已达10分钟上限，正在保存…");
          (void)meeting_start_worker(MEETING_JOB_STOP_SAVE, s_record_slot);
        }
    }
}

#endif /* CONFIG_DESKMATE_APP */
