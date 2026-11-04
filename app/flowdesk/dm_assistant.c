/* Local voice/text intent routing for FlowDesk. */

#include "deskmate.h"
#include "dm_voice.h"

#ifdef CONFIG_DESKMATE_APP

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define INTENT_TEXT_MAX 256
#define INTENT_REPLY_MAX 240
#define INTENT_WAIT_STEPS 160
#define INTENT_WAIT_US 50000
#define ASSISTANT_TODO_MAX_SECONDS (7 * 24 * 60 * 60)

typedef enum
{
  INTENT_NONE = 0,
  INTENT_FOCUS,
  INTENT_COUNTDOWN,
  INTENT_NOTE,
  INTENT_MOOD,
  INTENT_MOOD_RECORD,
  INTENT_MED_TAKE,
  INTENT_MBTI_OPEN,
  INTENT_WAKE,
  INTENT_GAME_ASK,
  INTENT_GAME,
  INTENT_GAME_CANCEL,
  INTENT_TODO_AFTER,
  INTENT_TODO_AT,
  INTENT_ALARM,
  INTENT_ALARM_AFTER,
  INTENT_TIME_ERROR
} assistant_intent_type_t;

typedef struct
{
  assistant_intent_type_t type;
  int seconds;
  int repeat_daily;
  dm_page_t game_page;
  int year;
  int month;
  int day;
  int hour;
  int minute;
  char text[INTENT_TEXT_MAX];
} assistant_intent_t;

static pthread_mutex_t s_intent_lock = PTHREAD_MUTEX_INITIALIZER;
static assistant_intent_t s_intent;
static int s_intent_active;
static int s_intent_executing;
static int s_intent_done;
static int s_waiting_game_choice;
static char s_intent_reply[INTENT_REPLY_MAX];

static int contains_any(const char *text, const char *const *words, size_t count)
{
  size_t i;
  for (i = 0; i < count; i++)
    {
      if (strstr(text, words[i]))
        {
          return 1;
        }
    }
  return 0;
}

static int chinese_digit(const char *p, size_t left, int *value)
{
  static const struct
  {
    const char *text;
    int value;
  } digits[] = {
    {"零", 0}, {"〇", 0}, {"一", 1}, {"二", 2}, {"两", 2},
    {"三", 3}, {"四", 4}, {"五", 5}, {"六", 6}, {"七", 7},
    {"八", 8}, {"九", 9}
  };
  size_t i;

  if (left < 3)
    {
      return 0;
    }
  for (i = 0; i < sizeof(digits) / sizeof(digits[0]); i++)
    {
      if (memcmp(p, digits[i].text, 3) == 0)
        {
          *value = digits[i].value;
          return 1;
        }
    }
  return 0;
}

static int chinese_unit(const char *p, size_t left, int *unit)
{
  if (left < 3)
    {
      return 0;
    }
  if (memcmp(p, "十", 3) == 0)
    {
      *unit = 10;
      return 1;
    }
  if (memcmp(p, "百", 3) == 0)
    {
      *unit = 100;
      return 1;
    }
  if (memcmp(p, "千", 3) == 0)
    {
      *unit = 1000;
      return 1;
    }
  return 0;
}

static int parse_number_slice(const char *text, size_t begin, size_t end,
                              int *value)
{
  size_t i;
  int ascii_only = 1;
  int total = 0;
  int section = 0;
  int number = -1;
  int saw = 0;

  if (!text || !value || begin >= end)
    {
      return 0;
    }
  for (i = begin; i < end; i++)
    {
      if (!isdigit((unsigned char)text[i]))
        {
          ascii_only = 0;
          break;
        }
    }
  if (ascii_only)
    {
      long parsed = 0;
      for (i = begin; i < end; i++)
        {
          int digit = text[i] - '0';
          if (parsed > (1000000 - digit) / 10)
            {
              return 0;
            }
          parsed = parsed * 10 + digit;
        }
      *value = (int)parsed;
      return 1;
    }

  i = begin;
  while (i < end)
    {
      int digit;
      int unit;
      if (chinese_digit(text + i, end - i, &digit))
        {
          number = digit;
          i += 3;
          saw = 1;
        }
      else if (chinese_unit(text + i, end - i, &unit))
        {
          int multiplier = number >= 0 ? number : 1;
          section += multiplier * unit;
          number = -1;
          i += 3;
          saw = 1;
        }
      else
        {
          return 0;
        }
    }
  if (!saw)
    {
      return 0;
    }
  if (number >= 0)
    {
      section += number;
    }
  total += section;
  if (total > 1000000)
    {
      return 0;
    }
  *value = total;
  return 1;
}

static int parse_number_before(const char *text, size_t end, size_t *start,
                               int *value)
{
  size_t begin;
  size_t number_end = end;

  while (number_end > 0 &&
         (text[number_end - 1] == ' ' || text[number_end - 1] == '\t' ||
          text[number_end - 1] == '\r' || text[number_end - 1] == '\n'))
    {
      number_end--;
    }
  begin = number_end;
  while (begin > 0)
    {
      int ignored;
      if (isdigit((unsigned char)text[begin - 1]))
        {
          begin--;
          continue;
        }
      if (begin >= 3 &&
          (chinese_digit(text + begin - 3, 3, &ignored) ||
           chinese_unit(text + begin - 3, 3, &ignored)))
        {
          begin -= 3;
          continue;
        }
      break;
    }
  if (!parse_number_slice(text, begin, number_end, value))
    {
      return 0;
    }
  if (start)
    {
      *start = begin;
    }
  return 1;
}

static const char *first_of(const char *text, const char *a, const char *b)
{
  const char *pa = a ? strstr(text, a) : NULL;
  const char *pb = b ? strstr(text, b) : NULL;
  if (!pa)
    {
      return pb;
    }
  if (!pb)
    {
      return pa;
    }
  return pa < pb ? pa : pb;
}

static int parse_duration_part(const char *text, const char *unit,
                               int unit_bytes, int multiplier,
                               int allow_half_hour, int max_seconds,
                               int *seconds)
{
  size_t start;
  int number;

  if (!unit)
    {
      return 0;
    }
  if (allow_half_hour && unit_bytes == 6 && unit >= text + 3 &&
      memcmp(unit - 3, "半", 3) == 0)
    {
      if (unit >= text + 6 && memcmp(unit - 6, "个", 3) == 0 &&
          parse_number_before(text, (size_t)(unit - text - 6), &start,
                              &number))
        {
          if (number < 0 || number > max_seconds / multiplier ||
              number * multiplier > max_seconds - 30 * 60)
            {
              return -1;
            }
          *seconds += number * multiplier + 30 * 60;
        }
      else
        {
          *seconds += 30 * 60;
        }
      return 1;
    }
  if (!parse_number_before(text, (size_t)(unit - text), &start, &number))
    {
      /* Accept the natural Mandarin form "一个小时". */
      if (allow_half_hour && unit_bytes == 6 && unit >= text + 3 &&
          memcmp(unit - 3, "个", 3) == 0 &&
          parse_number_before(text, (size_t)(unit - text - 3), &start,
                              &number))
        {
          /* parsed the number before 个 */
        }
      else
        {
          return -1;
        }
    }
  if (number < 0 || number > max_seconds / multiplier)
    {
      return -1;
    }
  *seconds += number * multiplier;
  return 1;
}

static int parse_duration_seconds(const char *text, int *seconds,
                                  int max_seconds)
{
  const char *hour_word = strstr(text, "小时");
  const char *minute_word = strstr(text, "分钟");
  const char *second_word = strstr(text, "秒");
  int total = 0;
  int parts = 0;
  int rc;

  if (!minute_word)
    {
      minute_word = strstr(text, "分");
    }
  rc = parse_duration_part(text, hour_word, 6, 60 * 60, 1,
                           max_seconds, &total);
  if (rc < 0)
    {
      return 0;
    }
  parts += rc;

  /* "半小时" is consumed as an hour component above; support an additional
   * half hour in forms such as "1小时半" as well. */
  if (!hour_word && strstr(text, "半小时"))
    {
      total += 30 * 60;
      parts++;
    }
  else if (hour_word && strstr(hour_word + 6, "半") &&
           (!minute_word || strstr(hour_word + 6, "半") < minute_word))
    {
      total += 30 * 60;
    }

  rc = parse_duration_part(text, minute_word,
                           minute_word && strstr(text, "分钟") == minute_word
                             ? 6 : 3,
                           60, 0, max_seconds, &total);
  if (rc < 0)
    {
      return 0;
    }
  parts += rc;

  rc = parse_duration_part(text, second_word, 3, 1, 0,
                           max_seconds, &total);
  if (rc < 0)
    {
      return 0;
    }
  parts += rc;

  if (parts == 0 || total < 1 || total > max_seconds)
    {
      return 0;
    }
  *seconds = total;
  return 1;
}

static int ascii_contains_ci(const char *text, const char *needle);
static int parse_datetime(const char *text, int *year, int *month, int *day,
                          int *hour, int *minute, size_t *date_start,
                          size_t *date_end, size_t *time_start,
                          size_t *time_end);

static const char *relative_marker(const char *text, size_t *bytes)
{
  const char *p = strstr(text, "之后");
  const char *q = strstr(text, "以后");
  const char *r = strstr(text, "后");
  const char *best = NULL;
  *bytes = 0;
  if (p) { best = p; *bytes = 6; }
  if (q && (!best || q < best)) { best = q; *bytes = 6; }
  if (r && (!best || r < best)) { best = r; *bytes = 3; }
  return best;
}

static void clean_relative_name(const char *text, char *out, size_t cap)
{
  const char *marker;
  const char *content;
  size_t marker_bytes;
  size_t len;
  static const char *const prefixes[] = {
    "提醒我", "提醒一下", "提醒", "叫醒我", "叫我", "帮我", "帮忙",
    "安排我", "记得", "请", "在", "我", "一下"
  };
  size_t i;
  if (!out || cap == 0)
    {
      return;
    }
  marker = relative_marker(text, &marker_bytes);
  content = marker ? marker + marker_bytes : NULL;
  if (!content)
    {
      snprintf(out, cap, "%s", "待办事项");
      return;
    }
  while (*content == ' ' || *content == '\t' || *content == ',' || *content == ':')
    {
      content++;
    }
  while (strncmp(content, "，", 3) == 0 || strncmp(content, "：", 3) == 0 ||
         strncmp(content, "、", 3) == 0)
    {
      content += 3;
    }
  while (*content)
    {
      int changed = 0;
      for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
        {
          size_t n = strlen(prefixes[i]);
          if (strncmp(content, prefixes[i], n) == 0)
            {
              content += n;
              changed = 1;
              break;
            }
        }
      if (!changed)
        {
          break;
        }
      while (*content == ' ' || *content == '\t' || *content == ',' || *content == ':')
        {
          content++;
        }
      while (strncmp(content, "，", 3) == 0 || strncmp(content, "：", 3) == 0 ||
             strncmp(content, "、", 3) == 0)
        {
          content += 3;
        }
    }
  len = strlen(content);
  while (len > 0)
    {
      if (len >= 3 && (memcmp(content + len - 3, "。", 3) == 0 ||
                       memcmp(content + len - 3, "，", 3) == 0 ||
                       memcmp(content + len - 3, "！", 3) == 0 ||
                       memcmp(content + len - 3, "？", 3) == 0))
        {
          len -= 3;
        }
      else if (content[len - 1] == ' ' || content[len - 1] == '\t' ||
               content[len - 1] == ',' || content[len - 1] == '.')
        {
          len--;
        }
      else
        {
          break;
        }
    }
  if (len == 0)
    {
      snprintf(out, cap, "%s", "待办事项");
      return;
    }
  if (len >= cap)
    {
      len = cap - 1;
      while (len > 0 && ((unsigned char)content[len] & 0xc0u) == 0x80u)
        {
          len--;
        }
    }
  memcpy(out, content, len);
  out[len] = '\0';
}

static int contains_alarm_word(const char *text)
{
  static const char *const words[] = {
    "闹钟", "alarm", "起床", "叫醒", "叫我", "每天叫我", "每日闹钟"
  };
  return contains_any(text, words, sizeof(words) / sizeof(words[0])) ||
         ascii_contains_ci(text, "alarm");
}

static void clean_alarm_name(const char *text, char *out, size_t cap)
{
  const char *time_mark = first_of(text, "点", "时");
  const char *minute_mark;
  const char *content;
  size_t len;
  static const char *const prefixes[] = {
    "叫醒我", "叫我", "提醒我", "闹钟", "请", "帮我", "设置", "设个"
  };
  size_t i;
  if (!time_mark)
    {
      snprintf(out, cap, "%s", "闹钟");
      return;
    }
  content = time_mark + 3;
  minute_mark = strstr(content, "分");
  if (minute_mark)
    {
      content = minute_mark + 3;
    }
  else if (strncmp(content, "半", 3) == 0)
    {
      content += 3;
    }
  while (*content == ' ' || *content == '\t' || *content == ',' || *content == ':')
    {
      content++;
    }
  while (strncmp(content, "，", 3) == 0 || strncmp(content, "：", 3) == 0 ||
         strncmp(content, "、", 3) == 0)
    {
      content += 3;
    }
  while (*content)
    {
      int changed = 0;
      for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
        {
          size_t n = strlen(prefixes[i]);
          if (strncmp(content, prefixes[i], n) == 0)
            {
              content += n;
              changed = 1;
              break;
            }
        }
      if (!changed) break;
      while (*content == ' ' || *content == '\t' || *content == ',' || *content == ':')
        {
          content++;
        }
      while (strncmp(content, "，", 3) == 0 || strncmp(content, "：", 3) == 0 ||
             strncmp(content, "、", 3) == 0)
        {
          content += 3;
        }
    }
  len = strlen(content);
  if (len == 0)
    {
      snprintf(out, cap, "%s", "闹钟");
      return;
    }
  if (len >= cap)
    {
      len = cap - 1;
      while (len > 0 && ((unsigned char)content[len] & 0xc0u) == 0x80u)
        {
          len--;
        }
    }
  memcpy(out, content, len);
  out[len] = '\0';
}

static int alarm_date_was_spoken(const char *text)
{
  const char *day_mark = first_of(text, "号", "日");
  const char *month_mark = strstr(text, "月");
  const char *year_mark = strstr(text, "年");
  size_t start;
  int value;

  if (day_mark && parse_number_before(text, (size_t)(day_mark - text),
                                      &start, &value))
    {
      return 1;
    }
  if (month_mark && parse_number_before(text, (size_t)(month_mark - text),
                                        &start, &value))
    {
      return 1;
    }
  if (year_mark && parse_number_before(text, (size_t)(year_mark - text),
                                       &start, &value))
    {
      return 1;
    }
  return 0;
}

static int parse_alarm_time(const char *text, assistant_intent_t *intent)
{
  size_t ds;
  size_t de;
  size_t ts;
  size_t te;
  const char *time_mark;
  const char *minute_mark;
  size_t hour_start;
  int hour;
  int minute = 0;
  int day_offset = 0;
  int next_day = 0;
  int has_datetime;
  time_t now;
  struct tm current;
  struct tm target;
  struct tm verify;
  time_t when;

  has_datetime = parse_datetime(text, &intent->year, &intent->month, &intent->day,
                                &intent->hour, &intent->minute, &ds, &de,
                                &ts, &te);
  if (has_datetime == 1)
    {
      const char *year_mark = strstr(text, "年");
      int explicit_year = year_mark != NULL;
      int explicit_month = strstr(text, "月") != NULL;
      int repeat_daily = strstr(text, "每天") || strstr(text, "每日") ||
                         strstr(text, "天天");
      int target_day = intent->day;
      int midnight_roll = 0;
      int attempt;

      if (explicit_year)
        {
          size_t year_start;
          int year_value;
          if (!parse_number_before(text, (size_t)(year_mark - text),
                                   &year_start, &year_value) ||
              year_value < 2024 || year_value > 2099)
            {
              return -2;
            }
          intent->year = year_value;
        }
      if ((strstr(text, "凌晨") || strstr(text, "早上") || strstr(text, "上午")) &&
          intent->hour == 12)
        {
          intent->hour = 0;
        }
      else if ((strstr(text, "下午") || strstr(text, "晚上") ||
                strstr(text, "傍晚") || strstr(text, "明晚")) &&
               intent->hour < 12)
        {
          intent->hour += 12;
        }
      else if ((strstr(text, "晚上") || strstr(text, "明晚")) &&
               intent->hour == 12)
        {
          intent->hour = 0;
          midnight_roll = 1;
        }

      now = time(NULL);
      if (!localtime_r(&now, &current) || current.tm_year + 1900 < 2024)
        {
          return -1;
        }
      if (!explicit_year)
        {
          intent->year = current.tm_year + 1900;
        }
      if (!explicit_month)
        {
          intent->month = current.tm_mon + 1;
        }
      for (attempt = 0; attempt < (explicit_year ? 1 : 13); attempt++)
        {
          memset(&target, 0, sizeof(target));
          target.tm_year = intent->year - 1900;
          target.tm_mon = intent->month - 1;
          target.tm_mday = target_day;
          target.tm_hour = 12;
          target.tm_isdst = -1;
          when = mktime(&target);
          if (when != (time_t)-1)
            {
              localtime_r(&when, &verify);
              if (verify.tm_year == intent->year - 1900 &&
                  verify.tm_mon == intent->month - 1 && verify.tm_mday == target_day)
                {
                  if (midnight_roll)
                    {
                      verify.tm_mday++;
                    }
                  verify.tm_hour = intent->hour;
                  verify.tm_min = intent->minute;
                  verify.tm_sec = 0;
                  verify.tm_isdst = -1;
                  when = mktime(&verify);
                  if (when != (time_t)-1 && when <= now && repeat_daily)
                    {
                      localtime_r(&when, &verify);
                      verify.tm_mday++;
                      verify.tm_hour = intent->hour;
                      verify.tm_min = intent->minute;
                      verify.tm_sec = 0;
                      verify.tm_isdst = -1;
                      when = mktime(&verify);
                    }
                  if (when != (time_t)-1)
                    {
                      localtime_r(&when, &target);
                      if (target.tm_hour == intent->hour &&
                          target.tm_min == intent->minute && when > now)
                        {
                          intent->year = target.tm_year + 1900;
                          intent->month = target.tm_mon + 1;
                          intent->day = target.tm_mday;
                          break;
                        }
                    }
                }
            }
          if (explicit_year)
            {
              return 0;
            }
          if (explicit_month)
            {
              intent->year++;
            }
          else
            {
              intent->month++;
              if (intent->month > 12)
                {
                  intent->month = 1;
                  intent->year++;
                }
            }
        }
      if (attempt >= (explicit_year ? 1 : 13))
        {
          return 0;
        }
      intent->repeat_daily = repeat_daily ? 1 : 0;
      clean_alarm_name(text, intent->text, sizeof(intent->text));
      return 1;
    }
  if (has_datetime < 0)
    {
      return -1;
    }
  if (alarm_date_was_spoken(text))
    {
      /* A spoken but invalid date must never silently become today's alarm. */
      return -2;
    }

  time_mark = first_of(text, "点", "时");
  if (!time_mark ||
      !parse_number_before(text, (size_t)(time_mark - text), &hour_start, &hour) ||
      hour < 0 || hour > 23)
    {
      return 0;
    }
  minute_mark = strstr(time_mark + 3, "分");
  if (minute_mark)
    {
      size_t minute_start;
      int parsed;
      if (!parse_number_before(text, (size_t)(minute_mark - text), &minute_start,
                               &parsed) ||
          minute_start < (size_t)(time_mark + 3 - text) || parsed < 0 || parsed > 59)
        {
          return 0;
        }
      minute = parsed;
    }
  else if (strncmp(time_mark + 3, "半", 3) == 0)
    {
      minute = 30;
    }

  if ((strstr(text, "凌晨") || strstr(text, "早上") || strstr(text, "上午")) &&
      hour == 12)
    {
      hour = 0;
    }
  else if ((strstr(text, "下午") || strstr(text, "晚上") ||
            strstr(text, "傍晚") || strstr(text, "明晚")) && hour < 12)
    {
      hour += 12;
    }

  now = time(NULL);
  if (!localtime_r(&now, &current) || current.tm_year + 1900 < 2024)
    {
      return -1;
    }
  if (strstr(text, "后天")) day_offset = 2;
  else if (strstr(text, "明天") || strstr(text, "明早") ||
           strstr(text, "明晚")) day_offset = 1;
  if (strstr(text, "晚上") && hour == 12)
    {
      hour = 0;
      next_day = 1;
    }
  else if (strstr(text, "夜里") && hour == 12)
    {
      hour = 0;
      next_day = 1;
    }
  memset(&target, 0, sizeof(target));
  target.tm_year = current.tm_year;
  target.tm_mon = current.tm_mon;
  target.tm_mday = current.tm_mday + day_offset + next_day;
  target.tm_hour = hour;
  target.tm_min = minute;
  target.tm_isdst = -1;
  when = mktime(&target);
  if (when == (time_t)-1)
    {
      return 0;
    }
  if (day_offset == 0 && when <= now &&
      !(strstr(text, "每天") || strstr(text, "每日") || strstr(text, "天天")))
    {
      target.tm_mday++;
      target.tm_isdst = -1;
      when = mktime(&target);
    }
  localtime_r(&when, &target);
  intent->year = target.tm_year + 1900;
  intent->month = target.tm_mon + 1;
  intent->day = target.tm_mday;
  intent->hour = hour;
  intent->minute = minute;
  intent->repeat_daily = strstr(text, "每天") || strstr(text, "每日") ||
                         strstr(text, "天天") ? 1 : 0;
  clean_alarm_name(text, intent->text, sizeof(intent->text));
  return 1;
}

static int ascii_contains_ci(const char *text, const char *needle)
{
  size_t n;
  const char *p;

  if (!text || !needle || !needle[0])
    {
      return 0;
    }
  n = strlen(needle);
  for (p = text; *p; p++)
    {
      size_t i;
      for (i = 0; i < n; i++)
        {
          unsigned char a = (unsigned char)p[i];
          unsigned char b = (unsigned char)needle[i];
          if (!a || tolower(a) != tolower(b))
            {
              break;
            }
        }
      if (i == n)
        {
          return 1;
        }
    }
  return 0;
}

static int is_wake_phrase(const char *text)
{
  int mentions_openvela = ascii_contains_ci(text, "openvela");
  if (!mentions_openvela)
    {
      return 0;
    }
  return strstr(text, "你好") != NULL ||
         (ascii_contains_ci(text, "hello") && mentions_openvela);
}

static int is_mbti_open_request(const char *text)
{
  static const char *const targets[] = {
    "mbti", "人格测试", "性格测试", "人格类型测试"
  };
  static const char *const actions[] = {
    "打开", "进入", "开始", "启动", "做", "测", "测试"
  };
  static const char *const questions[] = {
    "是什么意思", "是什么", "代表什么", "有什么含义"
  };
  int has_target = ascii_contains_ci(text, "mbti") ||
      contains_any(text, targets + 1,
                   sizeof(targets) / sizeof(targets[0]) - 1);

  if (!has_target || contains_any(text, questions,
                                  sizeof(questions) / sizeof(questions[0])))
    {
      return 0;
    }
  return contains_any(text, actions, sizeof(actions) / sizeof(actions[0]));
}

static int is_med_take_request(const char *text)
{
  static const char *const completed_words[] = {
    "吃了", "吃过", "吃完药", "吃药了", "服了", "服过", "服用过",
    "已服", "已吃", "已经吃", "已经服", "刚吃", "刚刚吃",
    "刚服", "刚刚服", "服药了", "服完药"
  };
  static const char *const context_words[] = {
    "药", "用药", "服用", "维生素", "胶囊", "药丸", "药水",
    "药品", "药物", "阿莫西林", "阿司匹林", "胰岛素"
  };
  static const char *const negated_words[] = {
    "没吃", "没有吃", "未吃", "没服", "没有服", "未服", "还没吃",
    "还没有吃", "还没服", "还没有服", "不吃", "不服", "漏服",
    "忘记吃", "忘了吃", "忘记服", "忘了服", "准备吃", "准备服",
    "要吃药", "要服药", "该吃药", "该服药", "记得吃药", "记得服药"
  };
  static const char *const question_words[] = {
    "吗", "有没有", "是否", "是不是", "要不要", "该不该", "？", "?"
  };
  static const char *const self_markers[] = {
    "我", "刚", "刚才", "刚刚", "已经", "已"
  };
  size_t i;

  if (contains_any(text, negated_words,
                   sizeof(negated_words) / sizeof(negated_words[0])) ||
      contains_any(text, question_words,
                   sizeof(question_words) / sizeof(question_words[0])))
    {
      return 0;
    }

  for (i = 0; i < sizeof(completed_words) / sizeof(completed_words[0]); i++)
    {
      const char *action = strstr(text, completed_words[i]);
      size_t action_len = strlen(completed_words[i]);
      size_t j;
      if (!action)
        {
          continue;
        }
      for (j = 0; j < sizeof(context_words) / sizeof(context_words[0]); j++)
        {
          const char *medicine = strstr(text, context_words[j]);
          if (!medicine)
            {
              continue;
            }
          if ((medicine < action + action_len &&
               medicine + strlen(context_words[j]) > action) ||
              (medicine >= action + action_len &&
               (size_t)(medicine - (action + action_len)) <= 6) ||
              (action >= medicine + strlen(context_words[j]) &&
               (size_t)(action - (medicine + strlen(context_words[j]))) <= 6))
            {
              return 1;
            }
        }
    }
  return contains_any(text, self_markers,
                     sizeof(self_markers) / sizeof(self_markers[0]));
}

static dm_page_t game_page_from_text(const char *text)
{
  if (strstr(text, "2048") || strstr(text, "二零四八") ||
      strstr(text, "两千零四十八"))
    {
      return PAGE_2048;
    }
  if (strstr(text, "猜数字") || strstr(text, "猜个数") ||
      strstr(text, "猜数"))
    {
      return PAGE_GUESS;
    }
  if (strstr(text, "24点") || strstr(text, "24 点") ||
      strstr(text, "二十四点"))
    {
      return PAGE_24;
    }
  return PAGE_COUNT;
}

static int is_game_request(const char *text)
{
  static const char *const triggers[] = {
    "小游戏", "玩游戏", "玩个游戏", "玩一局", "玩一下游戏", "一起玩",
    "想玩", "来一局"
  };
  return contains_any(text, triggers, sizeof(triggers) / sizeof(triggers[0]));
}

static int parse_datetime(const char *text, int *year, int *month, int *day,
                          int *hour, int *minute, size_t *date_start,
                          size_t *date_end, size_t *time_start,
                          size_t *time_end)
{
  const char *day_mark = first_of(text, "号", "日");
  const char *month_mark = strstr(text, "月");
  const char *time_mark;
  const char *minute_mark;
  const char *year_mark;
  size_t year_start = 0;
  size_t day_start;
  size_t hour_start;
  size_t month_start = 0;
  int explicit_month = 0;
  int explicit_year = 0;
  int month_value = 0;
  int year_value = 0;
  int day_value;
  int hour_value;
  int minute_value = 0;
  time_t now;
  struct tm current;
  struct tm target_tm;
  time_t target;
  struct tm verify;
  int attempt;
  int resolved = 0;

  if (!day_mark || !parse_number_before(text, (size_t)(day_mark - text),
                                        &day_start, &day_value) ||
      day_value < 1 || day_value > 31)
    {
      return 0;
    }
  if (month_mark && month_mark < day_mark)
    {
      if (!parse_number_before(text, (size_t)(month_mark - text), &month_start,
                               &month_value) || month_value < 1 ||
          month_value > 12)
        {
          return 0;
        }
      explicit_month = 1;
    }
  else
    {
      month_mark = NULL;
    }

  year_mark = month_mark ? strstr(text, "年") : NULL;
  if (year_mark && year_mark < month_mark)
    {
      if (!parse_number_before(text, (size_t)(year_mark - text), &year_start,
                               &year_value) || year_value < 2024 ||
          year_value > 2099)
        {
          return 0;
        }
      explicit_year = 1;
    }
  time_mark = first_of(day_mark + 3, "点", "时");
  if (!time_mark ||
      !parse_number_before(text, (size_t)(time_mark - text), &hour_start,
                           &hour_value) || hour_value < 0 || hour_value > 23)
    {
      return 0;
    }

  minute_value = 0;
  minute_mark = strstr(time_mark + 3, "分");
  if (minute_mark)
    {
      size_t minute_start;
      int parsed_minute;
      if (parse_number_before(text, (size_t)(minute_mark - text),
                              &minute_start, &parsed_minute) &&
          minute_start >= (size_t)(time_mark + 3 - text) &&
          parsed_minute >= 0 && parsed_minute <= 59)
        {
          minute_value = parsed_minute;
        }
      else
        {
          minute_mark = NULL;
        }
    }
  else if (strncmp(time_mark + 3, "半", 3) == 0)
    {
      minute_value = 30;
    }

  /* Resolve spoken 12-hour markers before choosing a future calendar date. */
  if ((strstr(text, "凌晨") || strstr(text, "早上") || strstr(text, "上午")) &&
      hour_value == 12)
    {
      hour_value = 0;
    }
  else if ((strstr(text, "下午") || strstr(text, "晚上") ||
            strstr(text, "傍晚") || strstr(text, "明晚")) && hour_value < 12)
    {
      hour_value += 12;
    }

  now = time(NULL);
  localtime_r(&now, &current);
  if (current.tm_year + 1900 < 2024)
    {
      return -1;
    }
  *year = explicit_year ? year_value : current.tm_year + 1900;
  *month = explicit_month ? month_value : current.tm_mon + 1;
  *day = day_value;
  *hour = hour_value;
  *minute = minute_value;

  /* For an omitted month, roll forward to the next valid occurrence. This
   * also handles dates such as the 31st when the current month is shorter. */
  for (attempt = 0; attempt < (explicit_year ? 1 : 12); attempt++)
    {
      memset(&target_tm, 0, sizeof(target_tm));
      target_tm.tm_year = *year - 1900;
      target_tm.tm_mon = *month - 1;
      target_tm.tm_mday = *day;
      target_tm.tm_hour = *hour;
      target_tm.tm_min = *minute;
      target_tm.tm_isdst = -1;
      target = mktime(&target_tm);
      if (target != (time_t)-1)
        {
          localtime_r(&target, &verify);
          if (verify.tm_year == *year - 1900 &&
              verify.tm_mon == *month - 1 && verify.tm_mday == *day &&
              verify.tm_hour == *hour && verify.tm_min == *minute &&
              (explicit_year || target > now))
            {
              resolved = 1;
              break;
            }
        }

      if (explicit_year)
        {
          break;
        }
      if (explicit_month)
        {
          (*year)++;
        }
      else
        {
          (*month)++;
          if (*month > 12)
            {
              *month = 1;
              (*year)++;
            }
        }
    }
  if (!resolved)
    {
      return 0;
    }

  if (date_start)
    {
      *date_start = explicit_year ? year_start
                                  : (explicit_month ? month_start : day_start);
    }
  if (date_end)
    {
      *date_end = (size_t)(day_mark - text) + 3;
    }
  if (time_start)
    {
      *time_start = hour_start;
    }
  if (time_end)
    {
      *time_end = (minute_mark ? (size_t)(minute_mark - text) + 3
                               : (size_t)(time_mark - text) + 3 +
                                 (minute_value == 30 ? 3 : 0));
    }
  return 1;
}

static void trim_reminder_punctuation(char *text, size_t *len)
{
  int changed = 1;
  while (*len > 0 && changed)
    {
      changed = 0;
      if (text[*len - 1] == ' ' || text[*len - 1] == '\t' ||
          text[*len - 1] == ',' || text[*len - 1] == ':' ||
          text[*len - 1] == '-' || text[*len - 1] == '.')
        {
          text[--(*len)] = '\0';
          changed = 1;
        }
      else if (*len >= 3 &&
               (memcmp(text + *len - 3, "。", 3) == 0 ||
                memcmp(text + *len - 3, "，", 3) == 0 ||
                memcmp(text + *len - 3, "！", 3) == 0 ||
                memcmp(text + *len - 3, "？", 3) == 0 ||
                memcmp(text + *len - 3, "；", 3) == 0 ||
                memcmp(text + *len - 3, "：", 3) == 0))
        {
          *len -= 3;
          text[*len] = '\0';
          changed = 1;
        }
    }
}

static void trim_reminder_clutter(char *text, size_t *len)
{
  static const char *const suffixes[] = {
    "提醒我", "提醒一下", "帮我提醒", "到时候", "请", "在"
  };
  size_t i;
  int changed = 1;

  while (*len > 0 && changed)
    {
      changed = 0;
      for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++)
        {
          size_t n = strlen(suffixes[i]);
          if (*len >= n && memcmp(text + *len - n, suffixes[i], n) == 0)
            {
              *len -= n;
              text[*len] = '\0';
              trim_reminder_punctuation(text, len);
              changed = 1;
              break;
            }
        }
    }
}

static void clean_todo_at_name(const char *text, char *out, size_t cap)
{
  size_t len;

  if (!out || cap == 0)
    {
      return;
    }

  clean_alarm_name(text, out, cap);
  if (strcmp(out, "闹钟") == 0)
    {
      snprintf(out, cap, "%s", "待办事项");
      return;
    }

  len = strlen(out);
  trim_reminder_clutter(out, &len);
  trim_reminder_punctuation(out, &len);
  if (len == 0)
    {
      snprintf(out, cap, "%s", "待办事项");
    }
}

static void clean_reminder_name(const char *text, size_t date_start,
                                size_t date_end, size_t time_start,
                                size_t time_end, char *out, size_t cap)
{
  char tmp[INTENT_TEXT_MAX];
  size_t len = strlen(text);
  size_t start = date_start;
  size_t end = date_end;
  size_t tstart = time_start;
  size_t tend = time_end;
  size_t out_len = 0;
  static const char *const leading[] = {
    "帮我设置提醒", "帮我", "给我", "提醒一下", "提醒我", "我提醒我", "倒数日",
    "记一个", "计一个", "记一下", "记下来", "在", "请", "把", "到时候"
  };
  size_t i;

  if (len >= sizeof(tmp))
    {
      snprintf(out, cap, "%s", "提醒事项");
      return;
    }
  memcpy(tmp, text, len);
  tmp[len] = '\0';

  if (tstart >= end)
    {
      memmove(tmp + tstart, tmp + tend, len - tend + 1);
      len -= tend - tstart;
      memmove(tmp + start, tmp + end, len - end + 1);
      len -= end - start;
    }
  else if (start >= tend)
    {
      memmove(tmp + start, tmp + end, len - end + 1);
      len -= end - start;
      memmove(tmp + tstart, tmp + tend, len - tend + 1);
      len -= tend - tstart;
    }
  else
    {
      memmove(tmp + start, tmp + end, len - end + 1);
      len -= end - start;
      if (tstart > end)
        {
          tstart -= end - start;
          tend -= end - start;
        }
      memmove(tmp + tstart, tmp + tend, len - tend + 1);
      len -= tend - tstart;
    }

  while (len > 0)
    {
      int changed = 0;
      while (len > 0 && (tmp[0] == ' ' || tmp[0] == '\t' || tmp[0] == ',' ||
                         tmp[0] == ':' || tmp[0] == '-'))
        {
          memmove(tmp, tmp + 1, len);
          len--;
        }
      while (len >= 3 &&
             (memcmp(tmp, "，", 3) == 0 || memcmp(tmp, "：", 3) == 0 ||
              memcmp(tmp, "、", 3) == 0))
        {
          memmove(tmp, tmp + 3, len - 2);
          len -= 3;
        }
      for (i = 0; i < sizeof(leading) / sizeof(leading[0]); i++)
        {
          size_t n = strlen(leading[i]);
          if (len >= n && memcmp(tmp, leading[i], n) == 0)
            {
              memmove(tmp, tmp + n, len - n + 1);
              len -= n;
              changed = 1;
              break;
            }
        }
      if (!changed)
        {
          break;
        }
    }
  trim_reminder_clutter(tmp, &len);
  trim_reminder_punctuation(tmp, &len);

  if (len == 0)
    {
      snprintf(out, cap, "%s", "提醒事项");
      return;
    }
  out_len = len < cap - 1 ? len : cap - 1;
  if (out_len < len)
    {
      while (out_len > 0 && ((unsigned char)tmp[out_len] & 0xc0u) == 0x80u)
        {
          out_len--;
        }
    }
  memcpy(out, tmp, out_len);
  out[out_len] = '\0';
}

static int parse_intent(const char *text, assistant_intent_t *intent)
{
  static const char *const note_triggers[] = {
    "帮我记一下", "帮我记下", "记一下", "记下来", "记下", "记一个",
    "记个", "备忘里"
  };
  static const char *const reminder_triggers[] = {
    "提醒", "闹钟", "倒数日", "记一下", "记一个", "计一个", "安排",
    "设置提醒", "设个提醒"
  };
  static const char *const mood_words[] = {
    "不开心", "不太开心", "不高兴", "不太高兴", "心情不好",
    "心情不太好", "心情差", "状态不好", "状态不太好", "情绪不好",
    "情绪不太好", "情绪低落", "低落", "提不起劲", "心累", "心烦",
    "难受", "难过", "伤心", "焦虑", "压力大", "压力很大", "有压力", "很累",
    "疲惫", "孤独", "烦躁", "沮丧"
  };
  static const char *const mood_state_words[] = {
    "不开心", "不太开心", "不高兴", "不太高兴", "心情不好",
    "心情不太好", "心情差", "状态不好", "状态不太好", "情绪不好",
    "情绪不太好", "情绪低落", "不太好", "不好受", "低落", "提不起劲",
    "心累", "心烦",
    "难受", "难过", "伤心", "焦虑", "压力大", "压力很大", "有压力", "很累",
    "疲惫", "孤独", "烦躁", "沮丧", "开心", "高兴", "愉快", "兴奋", "激动",
    "自信", "期待", "满足", "满意", "感动", "平静", "平和",
    "放松", "安心", "若有所思", "思考", "惊讶", "生气", "愤怒",
    "恼火", "累", "困", "紧张", "担心", "不安", "孤单", "有压力",
    "心情不错", "心情很好", "心情挺好", "心情一般", "感觉一般",
    "一般般", "心平气和",
    "坐立不安", "充满活力"
  };
  static const char *const mood_record_triggers[] = {
    "记录心情", "记录一下心情", "记录一下我的心情", "记下心情",
    "记一下心情", "记一下我的心情", "记录我的心情", "记下我的心情",
    "保存心情", "保存我的心情", "心情记录", "打卡心情",
    "帮我记心情", "帮我记录心情"
  };
  static const char *const mood_self_report[] = {
    "我现在", "我今天", "今天我", "最近我", "我最近", "我的心情",
    "我心情", "最近心情", "今天心情", "这几天心情", "我觉得",
    "我感觉", "今天感觉", "现在感觉", "我不太好", "我不好受",
    "我状态不好", "我状态不太好", "我很", "我有点", "我挺", "我好"
  };
  static const char *const no_reminder[] = {
    "不要提醒", "别提醒", "不用提醒", "取消提醒", "不要闹钟", "取消闹钟"
  };
  static const char *const no_focus[] = {
    "不要专注", "别专注", "不用专注", "取消专注", "不要计时", "别计时"
  };
  static const char *const todo_triggers[] = {
    "提醒", "待办", "记得", "到时候", "todo"
  };
  size_t date_start = 0;
  size_t date_end = 0;
  size_t time_start = 0;
  size_t time_end = 0;
  int has_dt;
  int seconds;
  int alarm_rc;
  size_t relative_bytes;
  int alarm_requested;
  dm_page_t game_page;
  int cancel_reminder;
  int cancel_focus;

  memset(intent, 0, sizeof(*intent));
  cancel_reminder = contains_any(
      text, no_reminder, sizeof(no_reminder) / sizeof(no_reminder[0]));
  cancel_focus = contains_any(text, no_focus,
                              sizeof(no_focus) / sizeof(no_focus[0]));

  has_dt = parse_datetime(text, &intent->year, &intent->month, &intent->day,
                          &intent->hour, &intent->minute, &date_start,
                          &date_end, &time_start, &time_end);
  relative_marker(text, &relative_bytes);
  alarm_requested = contains_alarm_word(text) ||
      ((strstr(text, "每天") || strstr(text, "每日") || strstr(text, "天天")) &&
       first_of(text, "点", "时") && !relative_bytes);
  if (!cancel_reminder && alarm_requested)
    {
      relative_marker(text, &relative_bytes);
      if (relative_bytes &&
          parse_duration_seconds(text, &seconds, ASSISTANT_TODO_MAX_SECONDS))
        {
          intent->type = INTENT_ALARM_AFTER;
          intent->seconds = seconds;
          clean_relative_name(text, intent->text, sizeof(intent->text));
          if (strcmp(intent->text, "待办事项") == 0)
            {
              snprintf(intent->text, sizeof(intent->text), "%s", "闹钟");
            }
          return 1;
        }
      alarm_rc = parse_alarm_time(text, intent);
      if (alarm_rc > 0)
        {
          intent->type = INTENT_ALARM;
          return 1;
        }
      if (alarm_rc < 0)
        {
          intent->type = INTENT_TIME_ERROR;
          return 1;
        }
    }

  if (!cancel_reminder && !alarm_requested && !cancel_focus &&
      !(strstr(text, "专注") || strstr(text, "计时") || strstr(text, "番茄钟")) &&
      contains_any(text, todo_triggers,
                   sizeof(todo_triggers) / sizeof(todo_triggers[0])))
    {
      const char *clock_mark = first_of(text, "点", "时");
      relative_marker(text, &relative_bytes);
      if (relative_bytes &&
          parse_duration_seconds(text, &seconds, ASSISTANT_TODO_MAX_SECONDS))
        {
          intent->type = INTENT_TODO_AFTER;
          intent->seconds = seconds;
          clean_relative_name(text, intent->text, sizeof(intent->text));
          return 1;
        }

      /* A clock-only request such as "11点提醒我开会" is a timed TODO,
       * not an alarm. Resolve it to the next occurrence of that local time. */
      if (!relative_bytes && has_dt == 0 && clock_mark)
        {
          alarm_rc = parse_alarm_time(text, intent);
          if (alarm_rc > 0)
            {
              intent->type = INTENT_TODO_AT;
              clean_todo_at_name(text, intent->text, sizeof(intent->text));
              return 1;
            }
          if (alarm_rc <= 0)
            {
              intent->type = INTENT_TIME_ERROR;
              return 1;
            }
        }
    }

  if (has_dt < 0 && !cancel_reminder &&
      contains_any(text, reminder_triggers,
                   sizeof(reminder_triggers) / sizeof(reminder_triggers[0])))
    {
      intent->type = INTENT_TIME_ERROR;
      return 1;
    }
  if (has_dt == 1 && !cancel_reminder &&
      contains_any(text, reminder_triggers,
                   sizeof(reminder_triggers) / sizeof(reminder_triggers[0])))
    {
      intent->type = INTENT_COUNTDOWN;
      clean_reminder_name(text, date_start, date_end, time_start, time_end,
                          intent->text, sizeof(intent->text));
      return 1;
    }

  if (is_med_take_request(text))
    {
      intent->type = INTENT_MED_TAKE;
      snprintf(intent->text, sizeof(intent->text), "%s", text);
      return 1;
    }
  if (is_mbti_open_request(text))
    {
      intent->type = INTENT_MBTI_OPEN;
      return 1;
    }

  game_page = game_page_from_text(text);
  if (game_page != PAGE_COUNT &&
      (is_game_request(text) || strstr(text, "玩") != NULL))
    {
      intent->type = INTENT_GAME;
      intent->game_page = game_page;
      return 1;
    }
  if (is_game_request(text))
    {
      intent->type = INTENT_GAME_ASK;
      return 1;
    }
  if (is_wake_phrase(text))
    {
      intent->type = INTENT_WAKE;
      return 1;
    }

  if (contains_any(text, mood_record_triggers,
                   sizeof(mood_record_triggers) /
                     sizeof(mood_record_triggers[0])))
    {
      intent->type = INTENT_MOOD_RECORD;
      intent->seconds = contains_any(
          text, mood_state_words,
          sizeof(mood_state_words) / sizeof(mood_state_words[0])) ? 0 : 1;
      snprintf(intent->text, sizeof(intent->text), "%s", text);
      return 1;
    }

  if (contains_any(text, note_triggers,
                   sizeof(note_triggers) / sizeof(note_triggers[0])))
    {
      const char *trigger = NULL;
      size_t trigger_len = 0;
      size_t i;
      for (i = 0; i < sizeof(note_triggers) / sizeof(note_triggers[0]); i++)
        {
          const char *found = strstr(text, note_triggers[i]);
          if (found && (!trigger || found < trigger))
            {
              trigger = found;
              trigger_len = strlen(note_triggers[i]);
            }
        }
      if (trigger)
        {
          const char *content = trigger + trigger_len;
          while (*content == ' ' || *content == '\t' || *content == ':' ||
                 *content == ',')
            {
              content++;
            }
          while (strncmp(content, "，", 3) == 0 ||
                 strncmp(content, "：", 3) == 0 ||
                 strncmp(content, "、", 3) == 0)
            {
              content += 3;
            }
          while (strncmp(content, "我", 3) == 0 ||
                 strncmp(content, "一下", 6) == 0 ||
                 strncmp(content, "记得", 6) == 0)
            {
              content += strncmp(content, "我", 3) == 0 ? 3 : 6;
            }
          if (!*content)
            {
              return 0;
            }
          intent->type = INTENT_NOTE;
          snprintf(intent->text, sizeof(intent->text), "%s", content);
          return 1;
        }
    }

  if (!cancel_focus &&
      (strstr(text, "专注") || strstr(text, "计时") || strstr(text, "番茄钟")) &&
      parse_duration_seconds(text, &seconds, 120 * 60))
    {
      intent->type = INTENT_FOCUS;
      intent->seconds = seconds;
      return 1;
    }

  if (contains_any(text, mood_state_words,
                   sizeof(mood_state_words) / sizeof(mood_state_words[0])) &&
      contains_any(text, mood_self_report,
                   sizeof(mood_self_report) / sizeof(mood_self_report[0])))
    {
      intent->type = INTENT_MOOD_RECORD;
      intent->seconds = 0;
      snprintf(intent->text, sizeof(intent->text), "%s", text);
      return 1;
    }

  if (contains_any(text, mood_words, sizeof(mood_words) / sizeof(mood_words[0])))
    {
      intent->type = INTENT_MOOD;
      snprintf(intent->text, sizeof(intent->text), "%s", text);
      return 1;
    }

  return 0;
}

static int run_intent(const assistant_intent_t *intent, char *reply, size_t cap)
{
  int rc;

  if (intent->type != INTENT_GAME_ASK && intent->type != INTENT_GAME &&
      intent->type != INTENT_GAME_CANCEL)
    {
      pthread_mutex_lock(&s_intent_lock);
      s_waiting_game_choice = 0;
      pthread_mutex_unlock(&s_intent_lock);
    }

  switch (intent->type)
    {
      case INTENT_FOCUS:
        rc = dm_focus_start_seconds(intent->seconds);
        if (rc == 0)
          {
            int minutes = intent->seconds / 60;
            int seconds = intent->seconds % 60;
            if (seconds == 0)
              {
                snprintf(reply, cap, "好，%d分钟专注已开始。我陪你。",
                         minutes);
              }
            else if (minutes == 0)
              {
                snprintf(reply, cap, "好，%d秒专注已开始。我陪你。", seconds);
              }
            else
              {
                snprintf(reply, cap,
                         "好，%d分钟%02d秒专注已开始。我陪你。",
                         minutes, seconds);
              }
            return 0;
          }
        snprintf(reply, cap, "已有专注计时正在进行，我没有重置它。先结束当前专注再试。");
        return -1;

      case INTENT_TODO_AFTER:
        rc = dm_todo_add_voice(intent->text, intent->seconds);
        if (rc == 0)
          {
            int hours = intent->seconds / 3600;
            int minutes = (intent->seconds % 3600) / 60;
            int seconds = intent->seconds % 60;
            dm_show(PAGE_TODO);
            if (hours > 0)
              {
                snprintf(reply, cap, "记好了：%s，%d小时%d分钟后提醒你，到时会语音播报。",
                         intent->text, hours, minutes);
              }
            else if (minutes > 0 && seconds > 0)
              {
                snprintf(reply, cap, "记好了：%s，%d分钟%02d秒后语音提醒你。",
                         intent->text, minutes, seconds);
              }
            else if (minutes > 0)
              {
                snprintf(reply, cap, "记好了：%s，%d分钟后语音提醒你。",
                         intent->text, minutes);
              }
            else
              {
                snprintf(reply, cap, "记好了：%s，%d秒后语音提醒你。",
                         intent->text, seconds);
              }
            return 0;
          }
        if (rc == -2)
          {
            snprintf(reply, cap, "待办列表已满，请先删除一项再添加。");
          }
        else if (rc == -4)
          {
            snprintf(reply, cap, "设备时间还没校准，暂时不能设定待办提醒。");
          }
        else
          {
            snprintf(reply, cap, "待办没有保存成功，请换一种说法再试。");
          }
        return -1;

      case INTENT_TODO_AT:
        rc = dm_todo_add_at_voice(intent->text, intent->year, intent->month,
                                  intent->day, intent->hour, intent->minute);
        if (rc == 0)
          {
            dm_show(PAGE_TODO);
            snprintf(reply, cap, "记好了：%s，%d月%d日%02d:%02d提醒你。",
                     intent->text, intent->month, intent->day, intent->hour,
                     intent->minute);
            return 0;
          }
        if (rc == -2)
          {
            snprintf(reply, cap, "待办列表已满，请先删除一项再添加。");
          }
        else if (rc == -4)
          {
            snprintf(reply, cap, "设备时间还没校准，暂时不能设定待办提醒。");
          }
        else
          {
            snprintf(reply, cap, "这个时间无效或已经过去，请说一个未来的时间。");
          }
        return -1;

      case INTENT_ALARM:
        rc = dm_alarm_add_voice(intent->text, intent->year, intent->month,
                                intent->day, intent->hour, intent->minute,
                                intent->repeat_daily);
        if (rc == 0)
          {
            dm_show(PAGE_ALARM);
            snprintf(reply, cap, "闹钟设好了：%s，%d月%d日%02d:%02d%s。",
                     intent->text, intent->month, intent->day, intent->hour,
                     intent->minute,
                     intent->repeat_daily ? "，每天重复" : "，响一次");
            return 0;
          }
        if (rc == -2)
          {
            snprintf(reply, cap, "闹钟数量已满，请先删除一个再添加。");
          }
        else if (rc == -4)
          {
            snprintf(reply, cap, "设备时间还没校准，暂时不能设闹钟。");
          }
        else
          {
            snprintf(reply, cap, "闹钟时间无效或已过，请换个时间再试。");
          }
        return -1;

      case INTENT_ALARM_AFTER:
        rc = dm_alarm_add_after_voice(intent->text, intent->seconds);
        if (rc == 0)
          {
            dm_show(PAGE_ALARM);
            snprintf(reply, cap, "闹钟已设好：%s，%d分钟后响铃。",
                     intent->text, (intent->seconds + 59) / 60);
            return 0;
          }
        if (rc == -2)
          {
            snprintf(reply, cap, "闹钟数量已满，请先删除一个再添加。");
          }
        else if (rc == -4)
          {
            snprintf(reply, cap, "设备时间还没校准，暂时不能设闹钟。");
          }
        else
          {
            snprintf(reply, cap, "闹钟没有保存成功，请再试一次。");
          }
        return -1;

      case INTENT_WAKE:
        dm_show(PAGE_CHAT);
        snprintf(reply, cap, "%s", "你好呀，有什么事可以帮到你？");
        return 0;

      case INTENT_MBTI_OPEN:
        dm_show(PAGE_MBTI);
        snprintf(reply, cap, "%s", "已打开 MBTI 测试，可以开始答题了。");
        return 0;

      case INTENT_MED_TAKE:
        rc = dm_med_voice_take(intent->text, reply, (int)cap);
        dm_show(PAGE_MED);
        return rc == 0 ? 0 : -1;

      case INTENT_GAME_ASK:
        pthread_mutex_lock(&s_intent_lock);
        s_waiting_game_choice = 1;
        pthread_mutex_unlock(&s_intent_lock);
        dm_show(PAGE_CHAT);
        snprintf(reply, cap,
                 "%s", "想玩什么小游戏？可以说2048、猜数字或者24点。");
        return 0;

      case INTENT_GAME:
        pthread_mutex_lock(&s_intent_lock);
        s_waiting_game_choice = 0;
        pthread_mutex_unlock(&s_intent_lock);
        dm_show(intent->game_page);
        if (intent->game_page == PAGE_2048)
          {
            snprintf(reply, cap, "%s", "好，我们来玩2048。游戏已经打开。");
          }
        else if (intent->game_page == PAGE_GUESS)
          {
            snprintf(reply, cap, "%s", "好，我们来玩猜数字。游戏已经打开。");
          }
        else
          {
            snprintf(reply, cap, "%s", "好，我们来玩24点。游戏已经打开。");
          }
        return 0;

      case INTENT_GAME_CANCEL:
        pthread_mutex_lock(&s_intent_lock);
        s_waiting_game_choice = 0;
        pthread_mutex_unlock(&s_intent_lock);
        dm_show(PAGE_CHAT);
        snprintf(reply, cap, "%s", "好，那我们先不玩。想玩的时候再告诉我。");
        return 0;

      case INTENT_COUNTDOWN:
        rc = dm_countdown_add_voice(intent->text, intent->year, intent->month,
                                    intent->day, intent->hour,
                                    intent->minute);
        if (rc == 0)
          {
            snprintf(reply, cap, "记好了：%s，%d月%d日%02d:%02d提醒你。",
                     intent->text, intent->month, intent->day, intent->hour,
                     intent->minute);
            return 0;
          }
        if (rc == -2)
          {
            snprintf(reply, cap, "倒数日列表已满，暂时不能添加。请先删除一项。");
          }
        else if (rc == -4)
          {
            snprintf(reply, cap, "设备日期时间还没校准，无法设置准确提醒。");
          }
        else
          {
            snprintf(reply, cap, "提醒没有保存成功，请检查日期后再试。");
          }
        return -1;

      case INTENT_NOTE:
        rc = dm_note_add_voice(intent->text);
        if (rc == 0)
          {
            snprintf(reply, cap, "%s", "记下了，已经保存到备忘。");
            return 0;
          }
        if (rc == -2)
          {
            snprintf(reply, cap, "备忘已满，暂时不能添加。请先删除一条。");
          }
        else
          {
            snprintf(reply, cap, "这条备忘没有保存成功，请再说短一点。");
          }
        return -1;

      case INTENT_MOOD_RECORD:
        rc = dm_health_record_mood_voice(intent->text);
        if (rc == 0)
          {
            dm_show(PAGE_HEALTH);
            if (intent->seconds == 0)
              {
                char advice[160];
                dm_health_voice_advice(intent->text, advice, sizeof(advice));
                snprintf(reply, cap, "记好了，心情已保存。%s", advice);
              }
            else
              {
                snprintf(reply, cap, "%s", "记好了，心情已保存到健康记录。");
              }
            return 0;
          }
        if (rc == -2)
          {
            if (intent->seconds == 0)
              {
                char advice[160];
                dm_health_voice_advice(intent->text, advice, sizeof(advice));
                snprintf(reply, cap, "心情暂未保存。%s", advice);
              }
            else
              {
                snprintf(reply, cap, "%s", "心情记录没有保存成功，请稍后重试。");
              }
            return -1;
          }
        if (intent->seconds)
          {
            dm_show(PAGE_MOOD_LOG);
            snprintf(reply, cap,
                     "%s", "我打开了心情记录页，请选择当前心情后点保存。");
            return 0;
          }
        dm_health_voice_advice(intent->text, reply, (int)cap);
        return 0;

      case INTENT_MOOD:
        dm_health_voice_advice(intent->text, reply, (int)cap);
        return 0;

      case INTENT_TIME_ERROR:
        snprintf(reply, cap, "设备时间未校准，或提醒时间无效；请检查日期时间后再试。");
        return -1;

      default:
        return -1;
    }
}

int dm_assistant_intent_try(const char *text, char *reply, int reply_cap)
{
  static const char *const cancel_game_words[] = {
    "算了", "不玩了", "取消", "先不了", "不用了"
  };
  assistant_intent_t parsed;
  dm_page_t game_page;
  int parsed_ok;
  int i;

  if (!text || !text[0] || !reply || reply_cap < 8)
    {
      return 0;
    }
  parsed_ok = parse_intent(text, &parsed);

  pthread_mutex_lock(&s_intent_lock);
  if (!parsed_ok && s_waiting_game_choice)
    {
      game_page = game_page_from_text(text);
      if (game_page != PAGE_COUNT)
        {
          memset(&parsed, 0, sizeof(parsed));
          parsed.type = INTENT_GAME;
          parsed.game_page = game_page;
          parsed_ok = 1;
        }
      else if (contains_any(text, cancel_game_words,
                            sizeof(cancel_game_words) /
                              sizeof(cancel_game_words[0])))
        {
          memset(&parsed, 0, sizeof(parsed));
          parsed.type = INTENT_GAME_CANCEL;
          parsed_ok = 1;
        }
    }
  if (!parsed_ok)
    {
      pthread_mutex_unlock(&s_intent_lock);
      return 0;
    }
  if (s_intent_active)
    {
      pthread_mutex_unlock(&s_intent_lock);
      return 0;
    }
  s_intent = parsed;
  s_intent_active = 1;
  s_intent_executing = 0;
  s_intent_done = 0;
  s_intent_reply[0] = '\0';
  pthread_mutex_unlock(&s_intent_lock);

  for (i = 0; i < INTENT_WAIT_STEPS; i++)
    {
      int done;
      usleep(INTENT_WAIT_US);
      pthread_mutex_lock(&s_intent_lock);
      done = s_intent_done;
      if (done)
        {
          int result = s_intent.type == INTENT_WAKE ? 2 : 1;
          snprintf(reply, (size_t)reply_cap, "%s", s_intent_reply);
          s_intent_active = 0;
          s_intent_done = 0;
          pthread_mutex_unlock(&s_intent_lock);
          return result;
        }
      pthread_mutex_unlock(&s_intent_lock);
    }

  pthread_mutex_lock(&s_intent_lock);
  if (!s_intent_executing)
    {
      s_intent_active = 0;
      s_intent_done = 0;
      pthread_mutex_unlock(&s_intent_lock);
      return 0;
    }
  pthread_mutex_unlock(&s_intent_lock);

  /* An operation already started on the LVGL thread; allow it to finish. */
  for (i = 0; i < 40; i++)
    {
      usleep(INTENT_WAIT_US);
      pthread_mutex_lock(&s_intent_lock);
      if (s_intent_done)
        {
          int result = s_intent.type == INTENT_WAKE ? 2 : 1;
          snprintf(reply, (size_t)reply_cap, "%s", s_intent_reply);
          s_intent_active = 0;
          s_intent_done = 0;
          pthread_mutex_unlock(&s_intent_lock);
          return result;
        }
      pthread_mutex_unlock(&s_intent_lock);
    }
  pthread_mutex_lock(&s_intent_lock);
  s_intent_active = 0;
  s_intent_executing = 0;
  s_intent_done = 0;
  pthread_mutex_unlock(&s_intent_lock);
  return 0;
}

void dm_assistant_intent_tick(void)
{
  assistant_intent_t intent;
  char reply[INTENT_REPLY_MAX];

  pthread_mutex_lock(&s_intent_lock);
  if (!s_intent_active || s_intent_executing || s_intent_done)
    {
      pthread_mutex_unlock(&s_intent_lock);
      return;
    }
  intent = s_intent;
  s_intent_executing = 1;
  pthread_mutex_unlock(&s_intent_lock);

  reply[0] = '\0';
  (void)run_intent(&intent, reply, sizeof(reply));

  pthread_mutex_lock(&s_intent_lock);
  snprintf(s_intent_reply, sizeof(s_intent_reply), "%s", reply);
  s_intent_done = 1;
  s_intent_executing = 0;
  pthread_mutex_unlock(&s_intent_lock);
}

#endif /* CONFIG_DESKMATE_APP */
