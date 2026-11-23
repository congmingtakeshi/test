import sys, os
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
from spectrum import analyze, load_dump

FS = 200000

def synth(freq=None, amp=3000, noise=300, n=40000, seed=0):
    rng = np.random.default_rng(seed)
    x = rng.normal(0, noise, n)
    if freq:
        x += amp * np.sin(2 * np.pi * freq * np.arange(n) / FS)
    return x.astype(np.int16)

def test_finds_40khz_peak_with_good_snr():
    peak, snr = analyze(synth(40000), FS)
    assert abs(peak - 40000) <= 1000
    assert snr >= 20

def test_noise_only_is_below_gate():
    _, snr = analyze(synth(None), FS)
    assert snr < 20

def test_off_band_tone_does_not_pass():
    peak, snr = analyze(synth(10000), FS)     # 10kHz tone, outside 30-50kHz search band
    assert snr < 20

def test_load_dump_reads_int16_le(tmp_path):
    p = tmp_path / "d.bin"
    np.array([1, -2, 300], dtype="<i2").tofile(p)
    assert list(load_dump(str(p))) == [1, -2, 300]

if __name__ == "__main__":
    import tempfile, pathlib
    test_finds_40khz_peak_with_good_snr(); test_noise_only_is_below_gate(); test_off_band_tone_does_not_pass()
    with tempfile.TemporaryDirectory() as d:
        test_load_dump_reads_int16_le(pathlib.Path(d))
    print("4 tests passed")
