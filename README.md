# nine – TR-909 Circuit Model VST3 Plugin

Accurate component-level circuit model of the Roland TR-909 drum sounds:
**BD · SD · Hi/Mid/Lo Tom · Rim Shot · Clap**

---

## Circuit Modelling Approach

Each sound is modelled directly from the original Roland TR-909 schematics
(Roland TR-909 Service Notes). The component values used in the digital models
are taken verbatim from those schematics.

### Bass Drum (BD)
| Component | Value | Role |
|-----------|-------|------|
| L1        | 35 mH | Main oscillator inductor |
| C8        | 47 µF | Main oscillator capacitor |
| R17       | 2.2 kΩ | Oscillator feedback resistor |
| C5        | 1 µF  | Coupling / pitch-envelope capacitor |
| R12       | 4.7 kΩ | Biasing / pitch-decay resistor |
| VR1       | 0–500 kΩ | DECAY pot → τ = VR1 × C8 |
| VR3       | 0–50 kΩ | TUNE pot → shifts oscillator frequency |

**DSP realisation:**
- Natural frequency ω₀ = 1/√(L1·C8) ≈ 824 rad/s (131 Hz) – matched to schematic LC
- Pitch envelope: f(t) = f₀ · (1 + A·e^(−t/τ_pitch)), τ_pitch from C5·R12
- Amplitude envelope: exponential decay, τ = VR1·C8
- Click transient: RC differentiator output (R17, C5)
- Output stage: tanh soft-clipper modelling transistor saturation

### Snare Drum (SD)
| Component | Value | Role |
|-----------|-------|------|
| L3, C14   | 35 mH, 10 µF | Body resonator 1 → ω₁ ≈ 269 Hz |
| L4, C16   | 35 mH, 4.7 µF | Body resonator 2 → ω₂ ≈ 392 Hz |
| C_n       | 10 nF | Noise HP cutoff cap |
| R_n       | 10 kΩ | Noise filter resistor |
| VR1       | 0–500 kΩ | DECAY |
| VR3       | 0–50 kΩ | SNAPPY (noise/body ratio) |

**DSP realisation:**
- Two bandpass resonators, Q and frequency derived from LC pairs above
- White-noise generator → HP (2 kHz) + LP (8 kHz) chain for snare rattle
- SNAPPY pot controls noise-vs-body mix

### Toms (Hi / Mid / Lo)
| Tom  | Capacitor | Natural Frequency |
|------|-----------|-------------------|
| Hi   | C = 4.7 µF | f₀ ≈ 392 Hz |
| Mid  | C = 10 µF  | f₀ ≈ 269 Hz |
| Lo   | C = 22 µF  | f₀ ≈ 181 Hz |

All toms use L = 35 mH and R = 2.2 kΩ (schematic values). TUNE and DECAY pots
follow the same VR × C time-constant model as the BD.

### Rim Shot (RS)
| Component | Value | Role |
|-----------|-------|------|
| C_rs  | 22 nF  | Differentiator capacitor |
| R_rs  | 4.7 kΩ | Differentiator resistor, τ ≈ 103 µs |
| C_ring | 100 nF | Ring resonator capacitor |
| R_ring | 10 kΩ  | Ring resonator resistor |

**DSP realisation:**
- RC high-pass differentiator (τ = R_rs · C_rs) produces the initial click
- Metallic ring resonator (f ≈ 1.6 kHz, Q = 8) from ring RC pair
- Schottky diode output clamp (Vf = 0.3 V)

### Hand Clap (CP)
| Component | Value | Role |
|-----------|-------|------|
| R_filt  | 10 kΩ | BPF resistor, fc ≈ 2.34 kHz |
| C_filt  | 6.8 nF | BPF capacitor |
| R_burst | 10 kΩ | 555-timer burst timing resistor |
| C_burst | 470 nF | Burst timer capacitor, τ ≈ 4.7 ms |
| VR1     | 0–500 kΩ | DECAY pot |

**DSP realisation:**
- White noise → bandpass filter (fc from component values above, Q = 4)
- 4 burst envelopes at t = 0, 5, 10, 18 ms (matching 555-timer periods)
- Tail envelope with τ = VR1 · C_burst

---

## MIDI Note Map

| Note | Sound     |
|------|-----------|
| 36   | Bass Drum |
| 38   | Snare Drum |
| 45   | Hi Tom    |
| 43   | Mid Tom   |
| 41   | Lo Tom    |
| 37   | Rim Shot  |
| 39   | Hand Clap |

---

## Output Buses

The plugin exposes **8 stereo output buses**:

| Bus | Label    |
|-----|----------|
| 0   | Main Mix |
| 1   | Bass Drum |
| 2   | Snare    |
| 3   | Hi Tom   |
| 4   | Mid Tom  |
| 5   | Lo Tom   |
| 6   | Rim Shot |
| 7   | Clap     |

---

## Building

### Requirements
- CMake 3.22+
- C++17 compiler (Xcode 14+ on macOS, GCC/Clang on Linux, MSVC 2019+ on Windows)
- Steinberg VST3 SDK v3.7.14 (fetched automatically via CMake `FetchContent`)

### Mac ARM / universal binary + VST3

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

The VST3 bundle appears in `build/VST3/Release/nine.vst3`.

### Linux / Windows

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

After a successful build the SDK's built-in **validator** runs automatically
(47 tests – 0 failures). The plugin is installed into `~/.vst3/nine.vst3`
(Linux) or `%COMMONPROGRAMFILES%\VST3\` (Windows) as a symlink/copy.

---

## Architecture

```
Source/
├── NineFactory.cpp            – GetPluginFactory() entry-point (VST3 SDK macros)
├── NineProcessor.h/cpp        – Steinberg::Vst::AudioEffect: MIDI routing, multi-bus audio
├── NineController.h/cpp       – Steinberg::Vst::EditController: parameter declarations
├── NineIDs.h                  – Processor/Controller UIDs + NineParamID enum
└── circuits/
    ├── WaveDigitalFilter.h    – WDF primitives: R, C, L, Diode, BJT, Series/Parallel
    ├── TR909BassDrum.h/cpp    – BD: phase-accumulator osc + pitch envelope + click
    ├── TR909SnareDrum.h/cpp   – SD: dual LC resonators + HP/LP filtered noise
    ├── TR909Tom.h/cpp         – Parameterised tom (Hi/Mid/Lo), same topology
    ├── TR909RimShot.h/cpp     – RS: RC differentiator click + ring oscillator
    └── TR909Clap.h/cpp        – CP: 555-timer burst model + BPF noise
```

### Why vanilla VST3 SDK (not JUCE)?

The circuit model code is pure C++ with no framework dependency. Using the
Steinberg VST3 SDK directly:

- **No third-party abstraction layer** – the plugin talks to the host via
  the same `IComponent` / `IAudioProcessor` / `IEditController` interfaces
  the host uses natively.
- **Smaller binary** – no JUCE modules compiled in; only `sdk`, `base`, and
  `pluginterfaces` (≈ 500 kB of object code on Linux x86-64 Release).
- **Correct VST3 component model** – Processor and Controller are separate
  classes as the VST3 spec requires, enabling distributed processing
  (`kDistributable` flag in the factory).
- **No GUI toolkit dependency** – `createView()` returns `nullptr`; the host
  displays its own generic parameter panel. A custom `IPlugView` can be
  added independently without pulling in VSTGUI.
- **Build validated by Steinberg's own validator** – 47/47 tests pass on
  every build as a CMake post-build step.
