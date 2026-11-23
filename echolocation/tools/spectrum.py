#!/usr/bin/env python3
"""Stage 0 check: does the microphone capture 40kHz? Usage: spectrum.py dump.bin fs_hz

dump.bin is raw little-endian int16 mono PCM. Prints the peak frequency in the
30-50kHz band and its height above the median spectrum level (dB). The plan's
gate is: peak within 40kHz +/-1kHz and at least 20dB above the noise floor.
"""
import sys
import numpy as np

BAND_HZ = (30000, 50000)
GATE_DB = 20.0


def load_dump(path):
    return np.fromfile(path, dtype="<i2")


def analyze(samples, fs_hz):
    x = np.asarray(samples, dtype=np.float64)
    x = x - x.mean()
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    freqs = np.fft.rfftfreq(len(x), 1.0 / fs_hz)
    floor = np.median(spec) + 1e-12
    in_band = (freqs >= BAND_HZ[0]) & (freqs <= BAND_HZ[1])
    if not in_band.any():          # fs too low to even reach the band
        return 0.0, float("-inf")
    idx = np.argmax(np.where(in_band, spec, 0.0))
    return float(freqs[idx]), float(10 * np.log10(spec[idx] / floor))


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    peak, snr = analyze(load_dump(argv[1]), float(argv[2]))
    ok = abs(peak - 40000) <= 1000 and snr >= GATE_DB
    print(f"peak={peak:.0f}Hz snr={snr:.1f}dB gate={'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
