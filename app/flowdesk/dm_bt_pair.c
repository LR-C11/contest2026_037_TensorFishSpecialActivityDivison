/*
 * dm_bt_pair.c — BLE pairing logic (board side)
 * Challenge-response authentication with shared key
 */

#include "deskmate.h"
#include "dm_bt_protocol.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>

static volatile int s_pair_state = 0; /* 0=unpaired, 1=pairing, 2=paired */

/* Simple challenge generator */
static uint32_t __attribute__((unused)) dm_bt_gen_challenge(void)
{
  return (uint32_t)(rand() ^ (uintptr_t)&s_pair_state);
}

/* Compute response from challenge + key */
static uint32_t dm_bt_compute_response(uint32_t challenge)
{
  uint32_t h = 0;
  const char *k = DM_BT_PAIR_KEY;
  int i;
  for (i = 0; i < DM_BT_PAIR_KEY_LEN; i++)
    h = h * 31 + (uint8_t)k[i];
  return challenge ^ h;
}

/* Verify response */
int dm_bt_verify_pair(uint32_t challenge, uint32_t response)
{
  uint32_t expected = dm_bt_compute_response(challenge);
  syslog(LOG_INFO, "[bt-pair] challenge=0x%x resp=0x%x expected=0x%x\n",
         challenge, response, expected);
  return (response == expected) ? 0 : -1;
}

/* Generate auth message: CMD + challenge */
int dm_bt_build_pair_req(uint8_t *buf, size_t buflen, uint32_t challenge)
{
  if (buflen < 6) return -1;
  buf[0] = DM_BT_CMD_PAIR_REQ;
  buf[1] = 4; /* data len */
  memcpy(buf + 2, &challenge, 4);
  return 6;
}

/* Parse auth response */
int dm_bt_parse_pair_resp(const uint8_t *buf, size_t len,
                          uint32_t *cmd, uint32_t *response)
{
  if (len < 6) return -1;
  *cmd = buf[0];
  if (buf[0] == DM_BT_CMD_PAIR_ACK && buf[1] == 4)
    {
      memcpy(response, buf + 2, 4);
      return 0;
    }
  return -1;
}

/* Build network on/off command */
int dm_bt_build_net_cmd(uint8_t *buf, size_t buflen, int on)
{
  if (buflen < 2) return -1;
  buf[0] = on ? DM_BT_CMD_NET_ON : DM_BT_CMD_NET_OFF;
  buf[1] = 0;
  return 2;
}

/* Build heartbeat */
int dm_bt_build_heartbeat(uint8_t *buf, size_t buflen)
{
  if (buflen < 2) return -1;
  buf[0] = DM_BT_CMD_HEARTBEAT;
  buf[1] = 0;
  return 2;
}
