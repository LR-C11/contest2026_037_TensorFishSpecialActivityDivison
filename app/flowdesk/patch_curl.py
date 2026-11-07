#!/usr/bin/env python3
"""Legacy one-off curl patcher; intentionally disabled.

The current FlowDesk implementation lives in dm_voice.c and dm_chat_ai.c.
It uses the chat-completions ASR/TTS schemas, runtime-only API credentials,
and NTP gating. Running the former string-replacement patch could restore
obsolete endpoints or reintroduce compile-time credentials.
"""

print("Legacy patch disabled; edit the current FlowDesk sources instead.")
