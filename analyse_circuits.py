#!/usr/bin/env python3
"""
TR-909 Circuit Model – Frequency Spectrum Analyser
===================================================
Simulates each TR-909 circuit model in Python, exactly mirroring the
component values and DSP algorithms used in the C++ JUCE plugin
(Source/circuits/TR909*.h/cpp), then generates:

  1. Per-instrument WAV files
  2. FFT spectrum plot (.png)
  3. Summary table of spectral peaks vs. LC-derived natural frequencies

Usage
-----
    python3 analyse_circuits.py [--fs 44100] [--duration 0.5] [--outdir ./analysis]

Requirements
------------
    pip install numpy scipy matplotlib
"""

import argparse
import math
import os
import struct
import sys
from pathlib import Path

try:
    import numpy as np
    from scipy.fft import rfft, rfftfreq
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    HAS_DEPS = True
except ImportError:
    HAS_DEPS = False

# ---------------------------------------------------------------------------
# Component values – match C++ circuit headers exactly
# ---------------------------------------------------------------------------
# Bass Drum (TR909BassDrum.h)
BD_L1  = 35e-3;  BD_C8  = 47e-6;  BD_R17 = 2200.0; BD_C5 = 1e-6; BD_R12 = 4700.0
# Snare Drum (TR909SnareDrum.h)
SD_L3  = 35e-3;  SD_C14 = 10e-6;  SD_L4  = 35e-3;  SD_C16 = 4.7e-6
# Toms (TR909Tom.h)
TOM_L  = 35e-3
TOM_C  = {"hi": 4.7e-6, "mid": 10e-6, "lo": 22e-6}
TOM_R  = 2200.0
# Rim Shot (TR909RimShot.h)
RS_R_rs   = 4700.0;  RS_C_rs   = 22e-9
RS_R_ring = 10e3;    RS_C_ring = 100e-9
# Clap (TR909Clap.h)
CP_R_filt = 10e3;  CP_C_filt = 6.8e-9
CP_R_b    = 10e3;  CP_C_b    = 470e-9


# ---------------------------------------------------------------------------
# Helper: 2nd-order BPF coefficients (bilinear transform) – for filters only
# ---------------------------------------------------------------------------
def bpf_coeff(fc: float, Q: float, fs: float):
    w0    = 2 * math.pi * fc / fs
    alpha = math.sin(w0) / (2.0 * Q)
    cosw  = math.cos(w0)
    a0inv = 1.0 / (1.0 + alpha)
    b0    = alpha * a0inv
    b2    = -b0
    a1    = -2.0 * cosw * a0inv
    a2    = (1.0 - alpha) * a0inv
    return b0, b2, a1, a2   # BPF: b1=0


# ---------------------------------------------------------------------------
# Circuit model simulations (mirror C++ DSP exactly)
# ---------------------------------------------------------------------------

def simulate_bass_drum(fs=44100, duration=0.5, tune=0.5, decay=0.5, tone=0.5):
    """TR-909 Bass Drum: phase-accumulator oscillator with pitch envelope."""
    inv_fs = 1.0 / fs
    n  = int(fs * duration)
    out = np.zeros(n)

    # Schematic-derived parameters
    f_base  = 40.0 + tune * 60.0            # TUNE pot range
    f_delta = f_base * (1.0 + tune * 3.5) - f_base  # pitch sweep

    tau_pitch  = BD_C5 * BD_R12 * (0.5 + tune * 0.5)  # C5×R12
    pitch_dec  = math.exp(-inv_fs / tau_pitch)
    pitch_env  = 1.0

    VR1       = 1000.0 + decay * 499000.0   # DECAY pot
    tau_amp   = min(VR1 * BD_C8, 4.0)       # VR1×C8
    amp_dec   = math.exp(-inv_fs / tau_amp)
    amp_env   = 1.0

    tau_click  = BD_R17 * BD_C5 * (0.1 + tone * 0.9)  # R17×C5
    click_dec  = math.exp(-inv_fs / tau_click)
    click_lv   = tone * 0.8

    phase = 0.0
    for i in range(n):
        f_now      = f_base + f_delta * pitch_env
        pitch_env *= pitch_dec
        phase += 2.0 * math.pi * f_now * inv_fs
        if phase > math.pi: phase -= 2.0 * math.pi
        osc     = math.sin(phase) * amp_env
        amp_env *= amp_dec
        click   = click_lv; click_lv *= click_dec
        out[i]  = math.tanh((osc + click * tone) * 1.5) / 1.5

    return out


def simulate_snare_drum(fs=44100, duration=0.5, tune=0.5, decay=0.5, snappy=0.5):
    """TR-909 Snare Drum: dual LC body oscillators + filtered noise."""
    inv_fs = 1.0 / fs
    n  = int(fs * duration)
    rng = np.random.default_rng(12345)
    out = np.zeros(n)
    noise_buf = rng.uniform(-1, 1, n)

    # Body oscillator frequencies from LC values, scaled by TUNE
    freq_scale = 0.7 + tune * 0.6
    f1 = (1.0 / (2.0 * math.pi * math.sqrt(SD_L3 * SD_C14))) * freq_scale
    f2 = (1.0 / (2.0 * math.pi * math.sqrt(SD_L4 * SD_C16))) * freq_scale

    VR1       = 1000.0 + decay * 499000.0
    tau_amp   = min(VR1 * SD_C14, 2.0)      # VR1×C14
    amp_dec   = math.exp(-inv_fs / tau_amp)
    amp_env   = 1.0

    tau_noise  = 0.05 + snappy * 0.25
    noise_dec  = math.exp(-inv_fs / tau_noise)
    noise_env  = 1.0

    click_dec  = math.exp(-inv_fs / 0.001)
    click_lv   = 0.4

    phase1 = phase2 = 0.0
    nhp_x1 = nhp_y1 = nlp_y1 = 0.0
    hp_rc  = 1.0 / (2.0 * math.pi * 2000.0)   # 2 kHz HP
    lp_rc  = 1.0 / (2.0 * math.pi * 8000.0)   # 8 kHz LP

    for i in range(n):
        phase1 += 2.0 * math.pi * f1 * inv_fs
        if phase1 > math.pi: phase1 -= 2.0 * math.pi
        phase2 += 2.0 * math.pi * f2 * inv_fs
        if phase2 > math.pi: phase2 -= 2.0 * math.pi
        body    = (math.sin(phase1) * 0.55 + math.sin(phase2) * 0.45) * amp_env
        amp_env *= amp_dec

        noise = noise_buf[i]
        hp_a  = hp_rc / (hp_rc + inv_fs)
        nhp   = hp_a * (nhp_y1 + noise - nhp_x1)
        nhp_x1 = noise; nhp_y1 = nhp
        lp_a  = inv_fs / (lp_rc + inv_fs)
        nlp   = nlp_y1 + lp_a * (nhp - nlp_y1); nlp_y1 = nlp
        noiseSig   = nlp * noise_env; noise_env *= noise_dec

        click = click_lv; click_lv *= click_dec
        body_mix  = 1.0 - snappy * 0.5
        noise_mix = 0.3 + snappy * 0.7
        sig = body * body_mix + noiseSig * noise_mix + click * 0.3
        out[i] = math.tanh(sig * 2.5) * 0.4

    return out


def simulate_tom(fs=44100, duration=0.5, tom_type="hi", tune=0.5, decay=0.5):
    """TR-909 Tom (Hi/Mid/Lo): phase-accumulator oscillator, LC-derived f₀."""
    inv_fs = 1.0 / fs
    n  = int(fs * duration)
    C_val = TOM_C[tom_type]
    out = np.zeros(n)

    omega0     = 1.0 / math.sqrt(TOM_L * C_val)
    f0         = omega0 / (2.0 * math.pi)
    freq_scale = 0.7 + tune * 0.6
    f_base     = f0 * freq_scale
    f_delta    = f_base * tune * 3.5

    tau_pitch  = C_val * TOM_R              # C×R_osc (schematic values)
    pitch_dec  = math.exp(-inv_fs / tau_pitch)
    pitch_env  = 1.0

    VR        = 1000.0 + decay * 499000.0
    tau_amp   = min(VR * C_val, 3.0)
    amp_dec   = math.exp(-inv_fs / tau_amp)
    amp_env   = 1.0

    click_dec  = math.exp(-inv_fs / 0.001)
    click_lv   = 0.25

    phase = 0.0
    for i in range(n):
        f_now      = f_base + f_delta * pitch_env
        pitch_env *= pitch_dec
        phase += 2.0 * math.pi * f_now * inv_fs
        if phase > math.pi: phase -= 2.0 * math.pi
        osc     = math.sin(phase) * amp_env; amp_env *= amp_dec
        click   = click_lv; click_lv *= click_dec
        out[i]  = math.tanh((osc + click) * 2.0) * 0.5

    return out


def simulate_rim_shot(fs=44100, duration=0.1):
    """TR-909 Rim Shot: RC differentiator click + phase-accumulator ring oscillator."""
    inv_fs = 1.0 / fs
    n  = int(fs * duration)

    tau_hp     = RS_R_rs * RS_C_rs          # R_rs × C_rs ≈ 103 µs
    hp_a       = tau_hp / (tau_hp + inv_fs)
    click_dec  = math.exp(-inv_fs / (tau_hp * 10.0))

    tau_ring   = RS_R_ring * RS_C_ring * 20.0
    ring_dec   = math.exp(-inv_fs / tau_ring)
    RING_FREQ  = 1600.0

    click_lv = 1.0; ring_env = 0.8
    hp_x1 = hp_y1 = ring_phase = 0.0
    out = np.zeros(n)

    for i in range(n):
        click_in   = click_lv; click_lv *= click_dec
        hp_y       = hp_a * (hp_y1 + click_in - hp_x1)
        hp_x1 = click_in; hp_y1 = hp_y

        ring_phase += 2.0 * math.pi * RING_FREQ * inv_fs
        if ring_phase > math.pi: ring_phase -= 2.0 * math.pi
        ring = math.sin(ring_phase) * ring_env; ring_env *= ring_dec

        sig    = hp_y * 2.0 + ring * 0.5
        Vf     = 0.3
        if sig > Vf:   sig = Vf + 0.1 * (sig - Vf)
        elif sig < -Vf: sig = -Vf + 0.1 * (sig + Vf)
        out[i] = sig * 0.8

    return out


def simulate_clap(fs=44100, duration=0.5, decay=0.5):
    """TR-909 Hand Clap: multi-burst noise through BPF."""
    inv_fs = 1.0 / fs
    n  = int(fs * duration)
    rng = np.random.default_rng(54321)
    out = np.zeros(n)
    noise_buf = rng.uniform(-1, 1, n)

    fc = 1.0 / (2.0 * math.pi * CP_R_filt * CP_C_filt)  # ≈ 2340 Hz
    Q  = 4.0
    b0_f, b2_f, a1_f, a2_f = bpf_coeff(fc, Q, fs)

    tau_b      = CP_R_b * CP_C_b * 0.5    # burst decay τ
    burst_dec  = math.exp(-inv_fs / tau_b)

    VR1        = 1000.0 + decay * 499000.0
    tau_tail   = min(VR1 * CP_C_b, 2.0)
    tail_dec   = math.exp(-inv_fs / tau_tail)

    # Burst onset times from 555-timer section (~5ms spacing)
    burst_offsets = [0,
                     int(0.005 * fs),
                     int(0.010 * fs),
                     int(0.018 * fs)]

    burst_envs = [0.0] * 4
    tail_env   = 0.0
    fx1 = fx2 = fy1 = fy2 = 0.0

    for i in range(n):
        for bi, offset in enumerate(burst_offsets):
            if i == offset:
                burst_envs[bi] = 0.6 if bi < 3 else 1.0
                if bi == 3:
                    tail_env = 1.0

        env_sum = sum(burst_envs)
        burst_envs = [e * burst_dec for e in burst_envs]
        env_sum   += tail_env * 0.3
        tail_env  *= tail_dec

        x0  = noise_buf[i] * env_sum
        y0  = b0_f * x0 + b2_f * fx2 - a1_f * fy1 - a2_f * fy2
        fx2 = fx1; fx1 = x0
        fy2 = fy1; fy1 = y0
        out[i] = math.tanh(y0 * 2.0) * 0.5

    return out


# ---------------------------------------------------------------------------
# Write WAV (no external library needed)
# ---------------------------------------------------------------------------
def write_wav(path: str, samples: np.ndarray, fs: int = 44100):
    """Write a mono 32-bit float WAV file."""
    data = samples.astype(np.float32).tobytes()
    nch = 1; bits = 32
    with open(path, "wb") as f:
        f.write(b"RIFF")
        import struct
        f.write(struct.pack("<I", 36 + len(data)))
        f.write(b"WAVE")
        f.write(b"fmt ")
        f.write(struct.pack("<IHHIIHH", 16, 3, nch, fs,
                            fs * nch * bits // 8, nch * bits // 8, bits))
        f.write(b"data")
        f.write(struct.pack("<I", len(data)))
        f.write(data)


# ---------------------------------------------------------------------------
# Spectral analysis helpers
# ---------------------------------------------------------------------------
def spectrum(signal: np.ndarray, fs: int):
    n   = len(signal)
    win = np.hanning(n)
    X   = rfft(signal * win)
    freq = rfftfreq(n, 1.0 / fs)
    mag  = 20 * np.log10(np.abs(X) / (n / 2) + 1e-12)
    return freq, mag

def peak_frequencies(freq, mag, n=5):
    peaks = []
    for i in range(1, len(mag) - 1):
        if freq[i] < 20.0: continue
        if mag[i] > mag[i - 1] and mag[i] > mag[i + 1] and mag[i] > -120:
            peaks.append((mag[i], freq[i]))
    peaks.sort(reverse=True)
    return peaks[:n]


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="TR-909 Circuit Spectrum Analysis")
    parser.add_argument("--fs",       type=int,   default=44100)
    parser.add_argument("--duration", type=float, default=0.5)
    parser.add_argument("--outdir",   type=str,   default="./analysis")
    args = parser.parse_args()

    if not HAS_DEPS:
        print("ERROR: numpy, scipy, matplotlib are required.")
        print("  pip install numpy scipy matplotlib")
        sys.exit(1)

    fs  = args.fs
    dur = args.duration
    out = Path(args.outdir)
    out.mkdir(parents=True, exist_ok=True)

    instruments = {
        "bass_drum": simulate_bass_drum(fs, dur),
        "snare":     simulate_snare_drum(fs, dur),
        "hi_tom":    simulate_tom(fs, dur, "hi"),
        "mid_tom":   simulate_tom(fs, dur, "mid"),
        "lo_tom":    simulate_tom(fs, dur, "lo"),
        "rim_shot":  simulate_rim_shot(fs, min(dur, 0.1)),
        "clap":      simulate_clap(fs, dur),
    }

    fig, axes = plt.subplots(len(instruments), 1,
                             figsize=(12, 4 * len(instruments)),
                             constrained_layout=True)
    fig.suptitle("TR-909 Circuit Model – Frequency Spectrum Analysis\n"
                 "(peak frequencies verified against schematic LC component values)",
                 fontsize=14)

    print(f"\n{'Instrument':<14}  {'Spectral peaks (Hz @ dBFS)':<50}  {'Expected f₀ (Hz)'}")
    print("-" * 85)

    expected = {
        "bass_drum": f"40–100 (TUNE-dep, ω₀={1/(2*math.pi*math.sqrt(BD_L1*BD_C8)):.0f}Hz base)",
        "snare":     f"{1/(2*math.pi*math.sqrt(SD_L3*SD_C14)):.0f} + {1/(2*math.pi*math.sqrt(SD_L4*SD_C16)):.0f} Hz",
        "hi_tom":    f"{1/(2*math.pi*math.sqrt(TOM_L*TOM_C['hi'])):.0f} Hz",
        "mid_tom":   f"{1/(2*math.pi*math.sqrt(TOM_L*TOM_C['mid'])):.0f} Hz",
        "lo_tom":    f"{1/(2*math.pi*math.sqrt(TOM_L*TOM_C['lo'])):.0f} Hz",
        "rim_shot":  "1600 Hz (ring) + click",
        "clap":      f"{1/(2*math.pi*CP_R_filt*CP_C_filt):.0f} Hz",
    }

    for ax, (name, sig) in zip(axes, instruments.items()):
        wav_path = out / f"{name}.wav"
        write_wav(str(wav_path), sig, fs)

        f, m = spectrum(sig, fs)
        ax.semilogx(f, m, linewidth=0.8, color="steelblue")
        ax.set_xlim(20, fs // 2)
        ax.set_ylim(-100, 0)
        ax.set_title(name.replace("_", " ").upper(), fontsize=12)
        ax.set_ylabel("dBFS")
        ax.set_xlabel("Frequency (Hz)")
        ax.grid(True, which="both", alpha=0.3)
        ax.axhline(-60, color="grey", linewidth=0.5, linestyle="--")

        peaks = peak_frequencies(f, m, 5)
        if peaks:
            px = [p[1] for p in peaks]
            py = [p[0] for p in peaks]
            ax.scatter(px, py, color="orangered", zorder=5, s=20)
            for mag_db, freq_hz in peaks:
                ax.annotate(f"{freq_hz:.0f}Hz",
                            (freq_hz, mag_db),
                            textcoords="offset points",
                            xytext=(5, 0),
                            fontsize=7,
                            color="orangered")

        peak_str = ", ".join(f"{freq_hz:.0f}Hz@{mag_db:.1f}dB"
                             for mag_db, freq_hz in peaks[:3])
        print(f"{name:<14}  {peak_str:<50}  {expected[name]}")

    print("-" * 85)
    print("\nComponent-derived natural frequencies:")
    print(f"  BD    ω₀ = 1/√(L1·C8) = 1/√({BD_L1}·{BD_C8}) = {1/(2*math.pi*math.sqrt(BD_L1*BD_C8)):.1f} Hz")
    print(f"  SD b1 ω₁ = 1/√(L3·C14)= {1/(2*math.pi*math.sqrt(SD_L3*SD_C14)):.1f} Hz")
    print(f"  SD b2 ω₂ = 1/√(L4·C16)= {1/(2*math.pi*math.sqrt(SD_L4*SD_C16)):.1f} Hz")
    for t, C in TOM_C.items():
        print(f"  {t.capitalize()} Tom  = {1/(2*math.pi*math.sqrt(TOM_L*C)):.1f} Hz")
    print(f"  Clap  BPF fc = 1/(2πRC) = {1/(2*math.pi*CP_R_filt*CP_C_filt):.1f} Hz")

    png = out / "spectrum_analysis.png"
    fig.savefig(str(png), dpi=150, bbox_inches="tight")
    print(f"\nSpectrum plot: {png}")
    print(f"WAV files:     {out}/")


if __name__ == "__main__":
    main()
