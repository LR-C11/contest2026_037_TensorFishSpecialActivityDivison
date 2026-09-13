# DeskMate (contest 037) — multi-page LVGL companion

## Files

| File | Role |
|------|------|
| `desk_companion.h` | Shared types, page enum, widgets |
| `desk_companion_main.c` | Entry only |
| `desk_state.c` | Fonts, `desk_show()` page switch |
| `desk_face.c` | 15 expressions + blink/sleep |
| `desk_ui_home.c` | Home face + chips |
| `desk_ui_wifi.c` | Wi-Fi page |
| `desk_ui_bt.c` | Bluetooth page |
| `desk_ui_note_about.c` | Notes + About |

## Build (must distclean after multi-file change)

```bash
cd /home/lrc/vela
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ distclean -j8
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8

cd vendor/allwinnertech/lichee/
source envsetup.sh
lunch_nuttx
pack
```

Log must show `DeskMate 0.6.0 pages=home/wifi/bt/note/about` — **not** `v0.2.0 UI ready`.
