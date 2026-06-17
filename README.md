# LiveDSP

A standalone, low-latency Windows audio application (JUCE 8) with a Neural DSP /
Fortin-style UI. **Two modes in a single `.exe`:** a landing screen at startup
lets you pick one of two modules — **GuitarDSP** (guitar amp simulator) or
**VoiceDSP** (live microphone channel). Each module has its own panel layout and
its own DSP signal chain; the shared theme, knob/panel widgets, and the tuner
are reused.

> It is not a plugin and does not play backing tracks — it only processes the
> incoming signal and sends it to the output for live monitoring.

## Designed for the Focusrite Scarlett Solo

LiveDSP is built first and foremost for the **Focusrite Scarlett Solo** (and
similar class-compliant USB audio interfaces) **with their firmware and ASIO
drivers already installed**. On such a device you plug a guitar into the
instrument input (or an SM58-style mic into the XLR input), select the
Focusrite **ASIO** driver, and monitor in real time at a low buffer size.

Other interfaces with a working ASIO driver work the same way. Without ASIO the
app still runs (WASAPI/DirectSound fallback), but at higher latency.

## Modes and signal chains

At the input you can **choose, per mode**, which interface channel to listen to
(the choice is remembered), so the microphone and the guitar never get mixed up.

### GuitarDSP module (guitar)
```
In → Input Gain → Noise Gate → Pitch (Transpose) → Overdrive
   → Amp (NAM) → Cab IR → EQ → Delay → Reverb → Output Gain → Out
```
The signal chain is mono up to the NAM stage and stereo afterwards. The NAM
model and the Cab IR are chosen directly from the **AMP/RIG** and **CAB** panel
drop-downs; the **AMP/RIG** panel also has a **Browse** button to import an
external `.nam` rig, and a download link (shown until the first model is loaded).
A LED on the **GATE** panel lights when the noise gate is ducking. 9-band
graphic EQ, a live latency readout, a visual tuner, and a preset selector.

**Pitch engines** (from the PITCH panel's engine selector): Signalsmith Stretch,
RubberBand Stretcher (R3 "Finer"), and **RubberBand LiveShifter (v4)** — the
default, for the lowest latency. Goal: −4 semitone power chords at good quality
with minimal latency.

### VoiceDSP module (vocals)
`juce::dsp::ProcessorChain`, strictly in this order:
```
In → Input Gain → Low-Cut (90 Hz) → Noise Gate → Warmth (tanh)
   → Compressor → High-Shelf "Air" (6 kHz) → Delay → Reverb → Brickwall Limiter → Out
```
The GATE and COMP panels get an activity LED (lit while ducking / compressing).
| Module | Control | Fixed setting |
|---|---|---|
| Input Gain | GAIN 0…+24 dB | — |
| Low-Cut | — | 90 Hz high-pass |
| Noise Gate | GATE −80…−20 dB | ratio 10:1, attack 2 ms, release 150 ms |
| Warmth | WARMTH 1.0…3.0 | level-preserving tanh saturation (WaveShaper) |
| Compressor | THRESH −40…0 dB, RATIO 1:1…10:1 | attack 5 ms, release 100 ms |
| High-Shelf "Air" | AIR 0…+12 dB | 6 kHz |
| Delay | TIME 50…500 ms, MIX 0…50% | feedback 0.3 |
| Reverb | MIX 0…100% | medium room (room/damp) |
| Limiter | — | −0.1 dB ceiling (clip protection) |

The whole vocal chain is **zero-latency**; all settings (every vocal parameter)
are persisted in the state. COMP/AIR/REVERB and GATE/WARMTH/DELAY can each be
toggled on/off.

## Models / rigs (NAM) and presets

NAM models/rigs and presets are **not bundled** with the app (commercial
captures cannot be redistributed). They live in a user-writable folder that is
created automatically on first launch:

```
Documents\LiveDSP\models     ← .nam models and .wav IRs
Documents\LiveDSP\favs       ← saved presets
```

### Getting a rig to try

A ready-made full rig to start with (Mesa Dual Rectifier, MW Red Modern, Mesa
4x12 — full rig):

**https://www.tone3000.com/tones/mesa-dual-rectifier-mw-red-modern-mesa-4x12-full-rig-69206**

Two ways to install it:

1. **Browse (easiest).** Launch LiveDSP → GuitarDSP, then on the **AMP/RIG**
   panel click **Browse**, pick the downloaded `.nam` file, and it is copied into
   `Documents\LiveDSP\models` and loaded automatically. (The download link on the
   AMP/RIG panel is shown only until the first model is loaded.)
2. **Manual copy.** Download and unzip the rig, then copy the `.nam` file(s)
   (and any `.wav` IRs) into:
   ```
   %USERPROFILE%\Documents\LiveDSP\models
   ```
   Restart LiveDSP (or reopen GuitarDSP) and pick the model from the AMP/RIG
   drop-down. The folder is created automatically the first time you launch the
   app, so it is already there waiting for your files.

> Many NAM captures are **"Full Rig"** type (they include the cabinet), so the
> **Cab IR is OFF by default** (no double cab). Only enable a separate IR for an
> "amp-only" (preamp/DI) NAM model.

## Dependencies

CMake `FetchContent` downloads automatically:
- **JUCE 8.0.4** (GPLv3 option)
- **Signalsmith Stretch** (pitch shifter, MIT)
- **RubberBand Library v4.0.0** (Stretcher + LiveShifter, single-file build, GPLv2-or-later)
- **NeuralAmpModelerCore** (+ Eigen, nlohmann/json)

Bundled in the repo:
- **Steinberg ASIO SDK** (`ext/ASIOSDK/`, **GPLv3** variant) — no manual download
  needed. (The trademarked Steinberg ASIO logo artwork is NOT part of the repo.)

## ASIO SDK

The Steinberg ASIO SDK ships in the repo under its GPLv3 variant
(`ext/ASIOSDK/`), so the build compiles with ASIO support out of the box — no
manual download. CMake finds `common/iasiodrv.h` automatically. To point at a
different SDK:
```
cmake -B build -G "Visual Studio 17 2022" -DLIVEDSP_ASIO_SDK_DIR="C:/SDKs/asiosdk"
```

> The build still works without ASIO (WASAPI/DirectSound fallback), but ASIO is
> required for low latency.

## Build (Visual Studio 2022)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The standalone application:
`build/LiveDSP_artefacts/Release/Standalone/LiveDSP.exe`

> Note: the source files are UTF-8; MSVC compiles with the `/utf-8` switch.

## Building the installer (distribution)

The finished app can be handed to any 64-bit Windows machine as a
**next-next-finish installer**. It is a single `.exe` that installs the
program and **requires no runtime** on the target machine (static MSVC runtime →
no Visual C++ Redistributable dependency). Models/rigs are not bundled; the user
adds them as described above.

### One-time prerequisite: Inno Setup

```powershell
winget install JRSoftware.InnoSetup
```

> `winget` and `ISCC.exe` are not necessarily on PATH. If CMake cannot find it,
> call it with a full path; a typical per-user location is
> `%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe`.

### Automatic build (recommended)

The installer is **rebuilt automatically at the end of every Release build** —
nothing extra to do. CMake locates `ISCC.exe` at configure time and compiles
`installer/LiveDSP.iss` after the standalone build.

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
# -> installer/Output/LiveDSP-Setup-<version>.exe
```

- Runs only in **Release** (Debug/incremental builds are untouched and stay fast).
- If Inno Setup is not installed, the build still succeeds; only the installer is skipped.
- Disable with: `cmake -B build -DLIVEDSP_BUILD_INSTALLER=OFF`.
- The version comes from `project(LiveDSP VERSION ...)` (CMake passes it to the `.iss`).

### Manual build

```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\LiveDSP.iss
```

### Installing on the target machine (end user)

1. Copy `LiveDSP-Setup-<version>.exe` to the other PC and run it → **Next →
   Next → Finish**. (Installing into Program Files requires admin; you get a
   Start-menu icon and, optionally, a desktop icon.)
2. Launch **LiveDSP** and pick **GuitarDSP** or **VoiceDSP** on the landing screen.
3. **Audio Settings** → choose the interface's **ASIO** driver and set a low buffer size.
4. Add a rig/model as described in **Models / rigs** above.

> **ASIO on the target machine:** low latency needs an ASIO driver there too. If
> the interface has its own ASIO driver (e.g. Focusrite Scarlett Solo), use it.
> Otherwise you can install the free [ASIO4ALL](https://asio4all.org) universal
> driver — the installer offers, in an **optional checkbox**, to open the
> **official ASIO4ALL download page** at the end (ASIO4ALL itself is not
> redistributed by the installer, as the author grants no public redistribution
> permission). Without ASIO the WASAPI fallback works, at higher latency.

## Testing

1. Launch the standalone `.exe` and pick **GuitarDSP** or **VoiceDSP** on the landing screen.
2. Audio Settings (Options) → choose the Focusrite **ASIO** driver and set a low buffer (64/128).
3. On the INPUT panel, select the correct input channel (guitar / microphone separately).
4. Use the "‹ MENU" button to go back and switch modes at any time.

## Architecture (in brief)

- `LiveDspProcessor` (a single `juce::AudioProcessor`): contains both DSP chains;
  a runtime-switchable `appMode` (none/guitar/voice) drives `processBlock`.
- `LiveDspEditor`: a thin shell that shows an `AppView` per mode
  (`LandingView` / `GuitarView` / `VoiceView`).
- DSP modules: `source/dsp/` (NoiseGate, Overdrive, PitchShifter, NamProcessor,
  CabConvolver, Equalizer, VoiceChain). UI: `source/ui/` (shared `LiveLookAndFeel` + panels).

## License

LiveDSP is distributed under the **GNU General Public License v3.0** (GPLv3) —
see the [LICENSE](LICENSE) file. This is required by the GPL dependencies (JUCE
under its GPLv3 option, Rubber Band GPLv2-or-later, Steinberg ASIO SDK GPLv3).
The third-party components and their licenses are listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

> The "ASIO" name and logo are **Steinberg** trademarks not covered by the GPL;
> LiveDSP does not use the name in its product/company name and does not
> redistribute the logo.

## Credits (icons)

- Landing guitar icon: [game-icons.net](https://game-icons.net) (lorc), **CC BY 3.0**.
- Landing microphone icon: [Phosphor Icons](https://phosphoricons.com) (microphone-stage), **MIT**.
