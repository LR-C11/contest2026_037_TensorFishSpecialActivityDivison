/*
 * dm_bt_protocol.h — Gemini-S1 BLE pairing protocol
 * Dedicated protocol for App Connect (not open to all devices)
 */

#ifndef DM_BT_PROTOCOL_H
#define DM_BT_PROTOCOL_H

/* ── 专属 Service UUID（只认这个） ───────────────────────── */
#define DM_BT_SVC_UUID      "12345678-1234-5678-1234-56789abcdef0"
#define DM_BT_CHAR_AUTH     "12345678-1234-5678-1234-56789abcdef1"
#define DM_BT_CHAR_CMD      "12345678-1234-5678-1234-56789abcdef2"
#define DM_BT_CHAR_STATUS   "12345678-1234-5678-1234-56789abcdef3"

/* ── 配对密钥（板子和 App 共享） ─────────────────────────── */
#define DM_BT_PAIR_KEY      "GEMINI-S1-2026"
#define DM_BT_PAIR_KEY_LEN  14

/* ── 协议命令 ─────────────────────────────────────────────── */
#define DM_BT_CMD_PAIR_REQ   0x01  /* App -> Board: 请求配对 */
#define DM_BT_CMD_PAIR_ACK   0x02  /* Board -> App: 配对确认 */
#define DM_BT_CMD_PAIR_FAIL  0x03  /* Board -> App: 配对失败 */
#define DM_BT_CMD_NET_ON     0x04  /* Board -> App: 开启网络共享 */
#define DM_BT_CMD_NET_OFF    0x05  /* Board -> App: 关闭网络共享 */
#define DM_BT_CMD_HEARTBEAT  0x06  /* 心跳保活 */

/* ── 消息格式 ─────────────────────────────────────────── */
/*
 * [CMD:1B] [LEN:1B] [DATA:0-32B]
 * 例: 01 0E GEMINI-S1-2026
 */

/* ── 配对验证 ─────────────────────────────────────────── */
/* 简单 challenge-response:
 * Board 发: DM_BT_CMD_PAIR_REQ + random 4B challenge
 * App 回:  challenge XOR key hash + DM_BT_CMD_PAIR_ACK
 */

#endif /* DM_BT_PROTOCOL_H */
