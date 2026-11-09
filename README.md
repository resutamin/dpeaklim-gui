# Disable Peak Limiter GUI

A GUI front-end (English / 日本語) for **dpeaklim** by Suguru Kawamoto — it disables the
always-on peak limiter in the Windows shared audio engine (`audioeng.dll` inside
`audiodg.exe`) so playback keeps its full dynamics instead of being pulled down above
−0.13 dB.

This fork adds a reliability layer so it works consistently on modern Windows
(tested on Windows 11 26H2 / build 26300), plus a GUI, a live status indicator,
tray/startup options and an anti-cheat guard.

> ⚠️ **Use at your own risk.** This loads a kernel driver and patches the running audio
> engine **in memory** (nothing on disk is modified; a reboot fully reverts it).

## Features

- **Red/green status** — shows at a glance whether the limiter is currently disabled.
- **Reliable apply** — plays silence to force `audioeng.dll` resident in RAM, waits,
  then patches, with automatic retry. Fixes the classic "sometimes it does nothing".
- **Safe default** — "limiter acts only above 0 dBFS" (no continuous glitching).
  A forced-off (∞) option is available but flagged as risky on some builds.
- **Crash-safe scan** — the physical-memory scan skips unreadable pages (SEH) instead
  of crashing.
- **Tray mode** — keep it running in the background; restore from the tray icon.
- **Run at login** — optional delayed (≈1 min) elevated scheduled task that applies
  automatically after you log in (not at boot — see the caveat below).
- **Anti-cheat guard** — warns before applying if a game with an anti-cheat
  (VRChat / EAC / BattlEye / Vanguard …) is running, because the driver will likely
  make the anti-cheat close the game.

## How it works

`audioeng.dll` stores the limiter threshold as the float `0.985` (−0.13 dB). The tool
reads the on-disk DLL, finds that constant, and rewrites the matching page in physical
RAM (via the InpOut32 driver) to a higher threshold so the limiter effectively never
engages. Because the patch lives only in memory, **a Windows restart restores the
original behaviour.**

## Usage

1. Run `dpeaklim_gui.exe` (it elevates via UAC — administrator is required).
2. Check the **Status** panel is all ✓ (if *Memory Integrity / Core Isolation* is ON,
   turn it off and reboot — otherwise the write is blocked).
3. Pick a threshold (default is recommended) and click **Disable limiter**.
4. The bar turns 🟩 green. Click **Restore** or reboot to go back to 🟥 normal.

**Anti-cheat games:** apply **before** launching them. The patch persists until reboot,
so you do not need to touch it again while the game is up. Do not apply while such a
game is running.

## Build

Requires Visual Studio 2022/Build Tools (C++ "Desktop development") + Windows 10/11 SDK.

```bat
build.bat
```

or open `dpeaklim_gui.sln` in Visual Studio (Release / x64). Output goes to `bin\`.
The build is a single self-contained exe (static CRT, no VC++ runtime needed); it only
requires `inpoutx64.dll` / `inpoutx64.sys` next to it at runtime.

## Credits & license

- Original **dpeaklim** © Suguru Kawamoto — MIT.
- GUI / reliability fixes / installer — MIT (see `LICENSE`).
- **InpOut32 / inpoutx64** driver © Logix4U & Highresolution Enterprises — third-party
  freeware, **not** MIT. See `NOTICE.txt`. If unsure about redistribution, link to the
  [official download](http://www.highrez.co.uk/Downloads/InpOut32) instead of committing
  the driver binaries.
- `audioeng.dll` is a Microsoft system file and is **never** included or redistributed.

## Disclaimer

This software patches a running Microsoft component through an unsigned-by-you kernel
driver. Antivirus and anti-cheat software may flag the InpOut32 driver. It is provided
"as is" with no warranty. You are responsible for how you use it.
