# ToneRig

A self-contained guitar rig — amp head, cabinet, stomp boxes, rack effects, tuner —
shipped as **one VST3 file**. Drop it in REAPER's FX chain and play. No wrapper, no
container plug-in, no installer, no separate host application.

ToneRig is an original implementation written from scratch for this project. It is
**not** Native Instruments Guitar Rig, is not a copy of it, and is not affiliated with
Native Instruments in any way. "Guitar Rig" is a trademark of Native Instruments.

---

## 1. What it does

```
                  ┌──────────────────────────────────────────────────────────────┐
  guitar DI  ──▶  │  input trim ▶ noise gate ▶ drive ▶ AMP ▶ CAB ▶ mod ▶ delay   │ ──▶  out
                  │                                 ▶ reverb ▶ EQ ▶ limiter      │
                  └──────────────────────────────────────────────────────────────┘
```

| Module | Controls | Notes |
|---|---|---|
| **Input / Gate** | In, Thresh, Release + on/off | soft-knee gate with hysteresis, no clicks |
| **Drive** | Drive, Tone, Level + on/off | TS-style overdrive: 720 Hz pre-filter, asymmetric clip, 800 Hz–8 kHz tone |
| **Amp Head** | Gain, Bass, Mid, Treble, Presence, Master + 5 models | Clean, Crunch, Lead, Fuzz, Acoustic |
| **Cabinet** | 5 voicings + Level | 4x12 V30, 4x12 Green, 2x12 Open, 1x12 Combo, Direct |
| **Modulation** | Type, Rate, Depth, Mix | Chorus, Flanger, Phaser (6-stage, with feedback) |
| **Delay** | Time, Feedback, Mix, Ping Pong | up to 1.3 s, smoothed time, analogue-ish feedback path |
| **Reverb** | Size, Damping, Width, Mix | Freeverb-style algorithmic reverb |
| **EQ** | Low, Mid, High | ±12 dB, pre/post amp styling |
| **Output** | Out, Limiter | peak limiter at ≈ −0.4 dBFS, zero lookahead |

Plus: a **chromatic tuner** (autocorrelation, ±1 cent in testing), input/output peak
meters with peak hold, **8 factory presets**, full host automation of all 42 parameters
and complete state save/restore.

**Zero added latency** — there is no lookahead anywhere in the chain.

## 2. "As a VSTi"

The plug-in is built with `JucePlugin_IsSynth=1`, so the module advertises the VST3
sub-categories `Instrument|Synth` and REAPER lists/labels it as a VST instrument.

It still declares a **stereo audio input bus** (verified: one main input bus, 2 channels),
so the track's audio is processed by it exactly the way an effect does. In REAPER an
instrument plug-in on a track receives that track's audio input, so:

1. Arm/monitor a track with your guitar DI, or put a recorded DI item on it.
2. `FX` → `Add FX` → `VST3` → `ToneRig`.
3. Play.

If your particular signal flow wants it filed under effects instead, configure with
`-DTONERIG_BUILD_AS_INSTRUMENT=OFF` and rebuild — everything else is identical.

## 3. Build it

Requirements: CMake ≥ 3.22, a C++17 compiler, Git. JUCE 8.0.15 is fetched automatically
into `third_party/JUCE`.

### Windows 11 (x64) — REAPER x64
1. Install, all free, all from the same machine:
   - **Visual Studio 2022** (Community, or the standalone **Build Tools**) with the
     **Desktop development with C++** workload ticked,
   - **CMake** (3.22 or newer - tick "Add CMake to the system PATH"),
   - **Git for Windows**.
   Close and reopen the terminal afterwards so `cmake` and `git` are on `PATH`.
2. Build:

```powershell
git clone <this repo> tonerig ; cd tonerig
.\scripts\build-windows.ps1
```

The script picks a CMake generator automatically (newest Visual Studio, else Ninja),
clones JUCE 8.0.15 into `third_party\JUCE` if it is missing, builds the VST3, then runs
the DSP test suite.

Output: `build\ToneRig_artefacts\Release\VST3\ToneRig.vst3`

3. Install it (needs an **Administrator** PowerShell), then restart/re-scan REAPER:

```powershell
.\scripts\build-windows.ps1 -Install          # copies into the common VST3 folder
```

or copy `build\ToneRig_artefacts\Release\VST3\ToneRig.vst3` by hand into
`C:\Program Files\Common Files\VST3\`, then in REAPER press **F5**, or
`Options → Preferences → Plug-ins → VST → Re-scan`.

Useful switches:

| Switch | Effect |
|---|---|
| `-Clean` | delete `build/` and start over |
| `-Install` | also copy the finished plug-in into `C:\Program Files\Common Files\VST3\` |
| `-Effect` | build as a plain effect instead of a VSTi |
| `-Generator "<name>"` | force a CMake generator instead of auto-detecting |
| `-Config Debug` | build the Debug configuration |

If PowerShell blocks the script ("running scripts is disabled on this system"):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

### macOS
Xcode command line tools + CMake.

```bash
./scripts/build-macos.sh          # universal (arm64 + x86_64)
```
Output: `build/ToneRig_artefacts/Release/VST3/ToneRig.vst3`
Install: copy to `~/Library/Audio/Plug-Ins/VST3/`.

### Linux
```bash
sudo apt install build-essential cmake ninja-build git \
     libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
     libxcomposite-dev libfreetype6-dev libfontconfig1-dev libasound2-dev
./scripts/build-linux.sh
```
Output: `build/ToneRig_artefacts/Release/VST3/ToneRig.vst3`
Install: `mkdir -p ~/.vst3 && cp -r build/.../ToneRig.vst3 ~/.vst3/`

### Useful CMake options

| Option | Default | Meaning |
|---|---|---|
| `TONERIG_BUILD_AS_INSTRUMENT` | `ON` | classify as VSTi; `OFF` makes it a plain effect |
| `TONERIG_BUILD_STANDALONE` | `OFF` | also build a standalone app (needs audio device support) |
| `TONERIG_BUILD_TESTS` | `ON` | build the DSP test and the VST3 host test |
| `TONERIG_COPY_PLUGIN` | `OFF` | copy the built plug-in into your user plug-in folder |

## 4. Tests

Two independent suites. `scripts/build-windows.ps1` and the other build scripts
run the first; the second also runs on Linux (and is skipped elsewhere, as it loads a
Unix `.so` through `dlopen`).

**`ToneRigDspTest`** — drives the DSP chain directly, no host involved (20 checks):
silence stays silent, bypassed chain is bit-transparent, all 5×5×3 amp/cab/mod
combinations stay finite and inside full scale, the gate opens/closes correctly, the
delay repeats at the requested time and ping-pongs, the reverb decays, the lead channel
squashes the crest factor, the tuner names 82.41 Hz as MIDI note 40, and the limiter
holds peaks at −0.4 dBFS.

**`ToneRigVst3HostTest`** (Linux) — `dlopen`s the finished `ToneRig.vst3` and talks to it
over the real VST3 ABI, the way a DAW does (32 checks): module entry points, factory,
class registration, controller/component connection, 1 stereo input bus + 1 output bus,
42 automatable parameters, editor view creation, bus activation, `process()`, audio
actually flowing, silence, and clean shutdown. This runs against the shipped binary, not
the source.

```bash
./build/ToneRigDspTest_artefacts/Release/ToneRigDspTest
./build/ToneRigVst3HostTest
```

## 5. Source layout

```
CMakeLists.txt              build definition + options
Source/
  PluginProcessor.{h,cpp}   parameters (APVTS), state, factory presets, bus setup
  PluginEditor.{h,cpp}      rack GUI, look and feel, header, tuner, meters
  dsp/
    RigDsp.h                biquads, smoothed values, delay line, tuner
    RigChain.{h,cpp}        the whole signal chain (host-independent, unit tested)
Tests/
  SmokeTest.cpp             offline DSP checks
  Vst3HostTest.cpp          VST3 ABI load/process checks
scripts/                    fetch JUCE + per-platform build & test
```

`RigChain` deliberately does not depend on `juce::AudioProcessor`, which is why the DSP
can be tested from a plain console binary.

## 6. Licensing note

JUCE 8 is dual-licensed (AGPLv3 or commercial). If you build and distribute this for
yourself, the AGPLv3 terms apply; JUCE also shows a splash screen unless you hold a
commercial licence. The plug-in code in this repository is yours to do as you like.
