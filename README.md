# EchoPod Enhanced (`echopod-enchanced`)

**EchoPod Enhanced** is a feature-extended fork of [EchoPod](https://github.com/BasilSkyWalk/echopod), the replacement firmware for the Snowsky (FiiO) Echo Mini portable music player (dual Cortex-M3 Rockchip RKnanoC).

This fork brings full **AAC-LC (`.m4a`)** and **Ogg Opus (`.opus`)** playback, full **Cyrillic and expanded CJK Unicode font coverage**, improved **power-on button handling**, and optimized Core 0 / Core 1 memory layouts while preserving rock-solid stability and zero-heap Core 1 architecture.

> [!NOTE]
> EchoPod Enhanced is community-developed open-source software and is not affiliated with or endorsed by FiiO / Snowsky.

---

## What's New in EchoPod Enhanced

### 1. AAC-LC Audio Playback (`.m4a`)
- **Helix AAC Decoder on Core 1:** Dedicated Core 1 overlay (`0x280000`) executing an optimized fixed-point Helix AAC engine.
- **Robust MP4 Parser:** Comprehensive atom traversal (`moov`, `trak`, `mdia`, `minf`, `stbl`, `stsd`, `mp4a`, `esds`, `stsz`, `stsc`, `stco`, `co64`). Supports files with metadata placed at the end of the file (`moov` after `mdat`), chunk-to-sample remapping, and arbitrary chunking.
- **Atomic Refills & Streaming:** Fully integrated with Core 0 / Core 1 ring buffer; handles starvation without dropping tracks prematurely.
- **UI Codec Indicator:** Shows `AAC` badge on the Now Playing screen.

### 2. Ogg Opus Audio Playback (`.opus`)
- **Official libopus 1.5.2 on Core 1:** High-quality fixed-point decoder running on a dedicated Core 1 overlay (`0x300000`).
- **Demuxer & Pre-skip Support:** Streaming Ogg demuxer correctly reassembles multi-segment packets (`seg_len == 255`), processes `OpusHead` headers, and strips encoder pre-skip delay.
- **Accurate Duration & Vorbis Tags:** Reads track length from the tail page granule position and extracts artist, title, album, track number, and genre from `OpusTags`.
- **48 kHz Native DAC Output:** Streams 16-bit stereo PCM directly into the hardware audio path.
- **Seamless Codec Switching:** Smooth queue transitions across FLAC, MP3, AAC, and Opus tracks.
- **UI Codec Indicator:** Shows `OPUS` badge on the Now Playing screen.

### 3. Power Button Boot Logic
- **Hold-to-Boot Detection:** Implements a state-machine debounce and hold check (`BOOT_POWER_HOLD_MS`) on boot. Prevents false power-ons from brief accidental presses in a pocket or bag.

### 4. Multilingual Unicode Typography
- **Cyrillic & Extended CJK Support:** Complete Cyrillic alphabet (Russian, Belarusian, Ukrainian) and expanded Chinese, Japanese, and Korean glyphs integrated into the compressed bitmap font system.
- **Case Folding & Sorting:** Fold table updated for proper Unicode case folding, searching, and alphabetical sorting in the music library.

### 5. Memory & Performance Measurements
- **Core 0 CODE_RAM:** 138,348 bytes used out of 157,292 limit (**+18.9 KB headroom** in Release build).
- **Core 1 AAC Overlay:** 50,540 bytes code (**+77 KB headroom**).
- **Core 1 Opus Overlay:** 73,160 bytes code, 27.7 KB BSS (**+57 KB code headroom**, **+13 KB BSS headroom**).
- **Test Suite:** 100% passed (126 / 126 CTest units).

---

## Supported Audio Formats

| Format | Extension | Container | Decoder Engine | Execution Core |
|---|---|---|---|---|
| **FLAC** | `.flac` | Native FLAC | Custom fixed-point | Core 1 |
| **MP3** | `.mp3` | MPEG-1/2 Audio Layer III | libmad fixed-point | Core 1 |
| **AAC-LC** | `.m4a` | MP4 / ISO Base Media | Helix AAC fixed-point | Core 1 |
| **Opus** | `.opus` | Ogg | libopus 1.5.2 fixed-point | Core 1 |
| **WAV** | `.wav` | RIFF WAVE PCM | Linear PCM unpacker | Core 0 |

---

## Building the Firmware

### Prerequisites

#### macOS
```sh
brew install cmake sdl2 clang-format arm-none-eabi-gcc arm-none-eabi-binutils
python3 -m venv .venv && .venv/bin/pip install -r tools/requirements.txt
```

#### Windows
Install [MSYS2](https://www.msys2.org) and open the **CLANG64** environment:
```sh
pacman -S mingw-w64-clang-x86_64-clang \
          mingw-w64-clang-x86_64-compiler-rt \
          mingw-w64-clang-x86_64-cmake \
          mingw-w64-clang-x86_64-ninja \
          mingw-w64-clang-x86_64-SDL2 \
          mingw-w64-clang-x86_64-arm-none-eabi-gcc \
          mingw-w64-clang-x86_64-arm-none-eabi-binutils \
          mingw-w64-clang-x86_64-arm-none-eabi-newlib
pip install -r tools/requirements.txt
```

### Compiling the Release Image

To build the flashable firmware image for the Echo Mini:

```sh
cmake --preset release
cmake --build --preset release
```

This compiles:
1. `echo_device.elf` (Core 0 main application)
2. `echo_rescan_overlay.elf` (Library rescan overlay)
3. `core1.bin` (Core 1 FLAC decoder)
4. `core1_mp3.bin` (Core 1 MP3 decoder)
5. `core1_aac.bin` (Core 1 AAC-LC decoder)
6. `core1_opus.bin` (Core 1 Opus decoder)

and packages everything into `build/release/HIFIEC90.IMG`.

To verify the generated firmware image:
```sh
./build/dev/tests/check_device_image ./build/release/HIFIEC90.IMG
```

---

## Running the Desktop Simulator

EchoPod includes an SDL2-based desktop simulator for UI development and testing:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# Launch simulator with your music folder
mkdir -p build/system
./build/dev/platform/sim/echo_sim --card /path/to/music --system-dir build/system
```

### Simulator Key Mappings

| Echo Mini Hardware Key | Simulator Key |
|---|---|
| **Volume Up** (menu navigation down) | Down Arrow |
| **Volume Down** (menu navigation up) | Up Arrow |
| **Play / Pause** | Return / Space |
| **Menu** | M / Tab |
| **Back** | Backspace / Escape |
| **Power** | P |

---

## Installing on the Device

1. Copy `build/release/HIFIEC90.IMG` to the root of a FAT32 or exFAT formatted microSD card.
2. Insert the card into your Echo Mini and power it on.
3. If booting EchoPod for the first time or updating via stock bootloader, follow the flashing steps in `docs/user-guide.md` or `docs/device-install.md`.
4. Put music tracks (`.flac`, `.mp3`, `.m4a`, `.opus`, `.wav`) in any folder structure on the card.
5. On boot, EchoPod scans the card into `/.echo/library.db` and indexes album covers into `/.echo/covers.bin`.

---

## Album Art Notes

- **MP3, FLAC, M4A:** Embedded front covers (ID3 APIC, FLAC Picture, MP4 `covr`) are extracted automatically during library rescan on the device.
- **Opus / Ogg:** Vorbis Comments store embedded covers as Base64-encoded FLAC blocks (`METADATA_BLOCK_PICTURE`). To display covers for Opus albums, place a `cover.jpg`, `folder.jpg`, `front.jpg`, or `albumart.jpg` (or `.png`) in the album folder, or pre-generate the cover cache on your PC using:
  ```sh
  python3 tools/cover_export.py --music /path/to/sdcard --library /path/to/sdcard/.echo/library.db --output /path/to/sdcard/.echo/covers.bin
  ```

---

## Navigation & Controls

| Button | In Menus & Lists | On Now Playing | During Seek Mode |
|---|---|---|---|
| **Vol+ / Vol−** | Move selection up / down | Tap: Volume adjust<br>Hold Vol+: Next track<br>Hold Vol−: Previous track | 5 s step per tap<br>15 s continuous step while held |
| **Play** | Select item<br>Hold on album: Add/Remove Favorite | Tap: Play / Pause<br>Hold: Enter Seek mode | Jump to selected time |
| **Back** | Return to previous screen<br>Hold: Jump to Home screen | Return to previous screen<br>Hold: Jump to Home screen | Cancel seek |
| **Power** | Tap: Screen on / off<br>Hold 2 s: Power off | Tap: Screen on / off<br>Hold 2 s: Power off | Tap: Screen on / off<br>Hold 2 s: Power off |

---

## Credits & License

- **EchoPod Core & UI:** GPL-3.0-or-later. Original project created by [Basil Joy](https://github.com/BasilSkyWalk/echopod).
- **Reverse Engineering & Hardware Research:** Built upon findings from the [ReChord](https://github.com/brnomt/ReChord) project (`docs/hardware.md`, `CREDITS.md`).
- **libopus:** BSD-3-Clause (Xiph.Org Foundation, Skype Technologies SA).
- **Helix AAC Decoder:** RealNetworks Public Source License / BSD-compatible embedded profile.
- **libmad:** GPL-2.0-or-later (Underbit Technologies, Inc.).
- **Inter Font:** SIL Open Font License 1.1 (`assets/fonts/inter/OFL.txt`).
