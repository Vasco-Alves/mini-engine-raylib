"""Synthesizes the demo project's jump sound (editor/assets/demo/audio/jump.wav).

The sound is generated rather than downloaded so every demo asset has a known
origin: a rising pitch sweep with a few odd harmonics and a fast decay.
Standard library only. Run from the repository root:

    python tools/make_jump_sound.py
"""
import math
import struct
import wave
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "editor" / "assets" / "demo" / "audio" / "jump.wav"

SAMPLE_RATE = 44100
DURATION = 0.32          # seconds
F_START, F_END = 260.0, 780.0
SWEEP = 0.22             # seconds to reach F_END
ATTACK, DECAY = 0.004, 0.09
FADE_OUT = 0.01
PEAK = 0.7               # about -3 dBFS


def synthesize():
    samples, phase = [], 0.0
    for i in range(int(SAMPLE_RATE * DURATION)):
        t = i / SAMPLE_RATE
        freq = F_START * (F_END / F_START) ** min(t / SWEEP, 1.0)
        phase += 2.0 * math.pi * freq / SAMPLE_RATE
        tone = math.sin(phase) + 0.1 * math.sin(3 * phase) + 0.03 * math.sin(5 * phase)
        envelope = min(t / ATTACK, 1.0) * math.exp(-t / DECAY) * min((DURATION - t) / FADE_OUT, 1.0)
        samples.append(tone * envelope)
    scale = PEAK / max(abs(s) for s in samples)
    return [s * scale for s in samples]


def main():
    samples = synthesize()
    with wave.open(str(OUT), "wb") as out:
        out.setnchannels(1)   # mono: the demo plays it as a spatial source
        out.setsampwidth(2)   # 16-bit PCM
        out.setframerate(SAMPLE_RATE)
        out.writeframes(b"".join(struct.pack("<h", round(s * 32767)) for s in samples))
    print(f"{OUT} ({len(samples) / SAMPLE_RATE:.2f} s)")


if __name__ == "__main__":
    main()
