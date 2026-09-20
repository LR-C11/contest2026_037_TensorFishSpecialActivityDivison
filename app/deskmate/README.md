# Deskmate (Gemini-S1 / OpenVela)

Version: **0.13.0**

## Features
- Focus timer: custom 1/5/10... + done page
- Notes: list/add/pinyin/persist
- Meds: keyboard + 1/2/3 times
- **2048** mini game
- **Chat**: mascot + text/voice, last 2 messages, MiMo/offline
- **MBTI**: 4 banks x 52 items, 4-choice 2x2, random bank
- WiFi auto-connect; Bluetooth UI removed

## Build
```bash
cd <openvela>
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
strings nuttx/vela.bin | grep Deskmate  # 0.13.0
```

Author: LR-C11 / carsonlinrui@gmail.com
