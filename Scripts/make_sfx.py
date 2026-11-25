# -*- coding: utf-8 -*-
"""
퍼즐 장치 효과음을 합성해서 WAV로 만든다 (외부 파일·라이브러리 없이 파이썬 표준 라이브러리만 사용).
배경음(노션 "배경음 (BGM)")처럼 직접 합성해서 저작권 걱정이 없다.

실행 방법 (언리얼에 들어 있는 파이썬으로 돌려도 됨):
  "<UE_5.3>/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" Scripts/make_sfx.py [출력 폴더]
  출력 폴더를 안 주면 <프로젝트>/Saved/SFX 에 만든다 (Saved는 git에 안 올라감).
그다음 Scripts/import_sfx.py 로 언리얼에 가져온다.

같은 시드로 만들어서 다시 돌려도 똑같은 소리가 나온다. 소리를 고치려면 아래 함수의 숫자만 바꾸면 된다.

  이름              쓰는 곳
  SFX_Click         버튼 누름 (PuzzleButton), 엘리베이터 호출
  SFX_Beep          키패드 숫자 누름
  SFX_Accept        카드 승인
  SFX_Deny          카드 거부, 키패드 틀림
  SFX_Success       키패드 정답, 회전 원판 완성
  SFX_LeverClunk    배전함 레버가 끝까지 내려감 (전원 들어옴)
  SFX_DoorOpen      문 열림/닫힘
  SFX_ElevatorDing  엘리베이터 도착
  SFX_PlateThud     압력판 눌림
  SFX_Dispense      디스펜서에서 큐브가 나옴
  SFX_Snap          코어 슬롯·도면 칸에 부품이 끼워짐
  SFX_RingTurn      회전 원판 고리 한 칸 돌아감
  SFX_PadChime      빛나는 발판 도착
  SFX_CoreShutdown  중앙 코어 정지 (5스테이지 클리어)
"""
import math
import os
import random
import struct
import sys
import wave

SR = 44100
TAU = 2.0 * math.pi


# ---------------------------------------------------------------- 기본 재료

def buf(sec):
    return [0.0] * int(sec * SR)


def mix(dst, src, at=0.0, gain=1.0):
    o = int(at * SR)
    n = min(len(src), len(dst) - o)
    for i in range(max(0, n)):
        dst[o + i] += src[i] * gain
    return dst


def env_exp(n, tau, attack=0.002):
    """짧은 어택 뒤 지수 감쇠 (tau초에 1/e)"""
    a = max(1, int(attack * SR))
    out = []
    for i in range(n):
        t = i / SR
        e = math.exp(-t / tau)
        out.append(e * (i / a if i < a else 1.0))
    return out


def tone(freq, sec, amp=1.0, tau=0.3, attack=0.002, shape="sine", freq_end=None):
    """freq에서 freq_end로 미끄러지는 음 (shape: sine / square / saw)"""
    n = int(sec * SR)
    env = env_exp(n, tau, attack)
    out = []
    phase = 0.0
    for i in range(n):
        f = freq if freq_end is None else freq + (freq_end - freq) * (i / n)
        phase += TAU * f / SR
        if shape == "sine":
            s = math.sin(phase)
        elif shape == "square":
            s = 1.0 if math.sin(phase) >= 0 else -1.0
            s *= 0.5
        else:  # saw
            s = ((phase / TAU) % 1.0) * 2.0 - 1.0
            s *= 0.5
        out.append(s * env[i] * amp)
    return out


def bell(freq, sec, amp=1.0, tau=0.6, partials=((1.0, 1.0), (2.0, 0.45), (2.76, 0.3), (5.4, 0.12))):
    """종소리: 어긋난 배음 여러 개 (높은 배음일수록 빨리 사라짐)"""
    out = buf(sec)
    for ratio, g in partials:
        mix(out, tone(freq * ratio, sec, amp * g, tau / (0.6 + ratio * 0.4)))
    return out


def noise(sec, amp=1.0, tau=0.2, attack=0.002, lp=None, hp=None, rng=None):
    rng = rng or random.Random(1)
    n = int(sec * SR)
    env = env_exp(n, tau, attack)
    x = [rng.uniform(-1.0, 1.0) for _ in range(n)]
    if lp:
        x = lowpass(x, lp)
    if hp:
        x = highpass(x, hp)
    return [x[i] * env[i] * amp for i in range(n)]


def lowpass(x, fc):
    a = 1.0 - math.exp(-TAU * fc / SR)
    y, out = 0.0, []
    for s in x:
        y += a * (s - y)
        out.append(y)
    return out


def highpass(x, fc):
    low = lowpass(x, fc)
    return [x[i] - low[i] for i in range(len(x))]


def finish(x, peak_db=-3.0):
    """양 끝을 살짝 페이드하고 최대 음량을 맞춘다"""
    fade = int(0.004 * SR)
    for i in range(min(fade, len(x))):
        x[i] *= i / fade
        x[-1 - i] *= i / fade
    peak = max(1e-9, max(abs(s) for s in x))
    g = (10 ** (peak_db / 20.0)) / peak
    return [s * g for s in x]


# ---------------------------------------------------------------- 효과음

def sfx_click():
    x = buf(0.12)
    mix(x, noise(0.05, 1.0, tau=0.006, hp=1500, rng=random.Random(11)))
    mix(x, tone(2400, 0.05, 0.5, tau=0.008))
    mix(x, tone(180, 0.08, 0.5, tau=0.02))
    return finish(x, -6)


def sfx_beep():
    x = tone(1250, 0.09, 1.0, tau=0.2, attack=0.003, shape="square")
    return finish(lowpass(x, 5000), -9)


def sfx_accept():
    x = buf(0.42)
    mix(x, tone(880, 0.16, 1.0, tau=0.5, shape="square"))
    mix(x, tone(1320, 0.24, 1.0, tau=0.5, shape="square"), at=0.15)
    return finish(lowpass(x, 4500), -8)


def sfx_deny():
    n = int(0.5 * SR)
    x = tone(160, 0.5, 1.0, tau=2.0, attack=0.005, shape="saw")
    # 두 번 끊기는 경고음
    for i in range(n):
        t = i / SR
        x[i] *= 1.0 if (t < 0.2 or 0.26 < t < 0.46) else 0.0
    return finish(lowpass(x, 2200), -7)


def sfx_success():
    x = buf(1.3)
    # 레 – 라 – 레 (배경음 모티프와 같은 레·라)
    for at, f in ((0.0, 587.33), (0.12, 880.0), (0.24, 1174.66)):
        mix(x, bell(f, 1.0, 0.8, tau=0.7), at=at)
    return finish(x, -6)


def sfx_lever_clunk():
    rng = random.Random(21)
    x = buf(1.6)
    mix(x, tone(70, 0.5, 1.0, tau=0.12, freq_end=45))                        # 묵직한 쿵
    mix(x, noise(0.25, 0.7, tau=0.05, lp=1800, rng=rng))                     # 금속 부딪힘
    mix(x, bell(1900, 0.6, 0.25, tau=0.15))                                  # 쇳소리 울림
    mix(x, noise(0.03, 0.6, tau=0.005, hp=2500, rng=rng), at=0.18)           # 릴레이 딸깍
    hum = buf(1.3)
    for i in range(len(hum)):
        t = i / SR
        swell = min(1.0, t / 0.6) * math.exp(-max(0.0, t - 0.7) / 0.35)
        hum[i] = (math.sin(TAU * 60 * t) + 0.5 * math.sin(TAU * 120 * t) + 0.25 * math.sin(TAU * 180 * t)) * swell * 0.35
    mix(x, hum, at=0.25)                                                     # 전원 들어오는 웅
    return finish(x, -3)


def sfx_door_open():
    rng = random.Random(31)
    sec = 1.1
    x = buf(sec)
    n = len(x)
    whir = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        f = 110 + 70 * min(1.0, t / 0.3)
        phase += TAU * f / SR
        e = min(1.0, t / 0.08) * (1.0 if t < 0.85 else max(0.0, 1 - (t - 0.85) / 0.25))
        whir.append((math.sin(phase) + 0.4 * math.sin(phase * 2) + 0.2 * math.sin(phase * 3)) * e * 0.4)
    mix(x, lowpass(whir, 1200))
    hiss = noise(0.5, 0.35, tau=0.18, attack=0.01, hp=2500, rng=rng)        # 공기 빠지는 칙
    mix(x, hiss, at=0.0)
    mix(x, tone(90, 0.2, 0.6, tau=0.06), at=0.88)                            # 끝에 멈추는 턱
    return finish(x, -5)


def sfx_elevator_ding():
    x = buf(1.8)
    mix(x, bell(1318.5, 1.6, 0.9, tau=0.9))         # 미
    mix(x, bell(1046.5, 1.4, 0.9, tau=0.9), at=0.4) # 도
    return finish(x, -5)


def sfx_plate_thud():
    rng = random.Random(41)
    x = buf(0.8)
    mix(x, tone(55, 0.6, 1.0, tau=0.15, freq_end=38))
    mix(x, noise(0.3, 0.6, tau=0.06, lp=900, rng=rng))
    mix(x, bell(820, 0.5, 0.2, tau=0.12), at=0.01)
    mix(x, noise(0.03, 0.4, tau=0.004, hp=3000, rng=rng), at=0.12)          # 걸림쇠 딸깍
    return finish(x, -3)


def sfx_dispense():
    rng = random.Random(51)
    x = buf(0.7)
    mix(x, noise(0.25, 0.8, tau=0.08, attack=0.01, lp=3000, hp=400, rng=rng))   # 푸슉
    mix(x, tone(120, 0.3, 0.8, tau=0.07, freq_end=80), at=0.18)                 # 툭 떨어짐
    mix(x, noise(0.06, 0.4, tau=0.02, lp=1500, rng=rng), at=0.18)
    return finish(x, -5)


def sfx_snap():
    rng = random.Random(61)
    x = buf(0.9)
    mix(x, noise(0.04, 1.0, tau=0.006, hp=2000, rng=rng))                   # 착
    mix(x, bell(2200, 0.4, 0.3, tau=0.1))
    mix(x, tone(95, 0.15, 0.5, tau=0.04))
    mix(x, tone(220, 0.7, 0.25, tau=0.5, attack=0.08, freq_end=440), at=0.06)  # 에너지 차오름
    return finish(x, -4)


def sfx_ring_turn():
    rng = random.Random(71)
    x = buf(0.3)
    for k, at in enumerate((0.0, 0.07, 0.14)):
        mix(x, noise(0.03, 1.0 - k * 0.15, tau=0.004, hp=1800, rng=rng), at=at)
        mix(x, tone(1500 + k * 120, 0.03, 0.25, tau=0.006), at=at)
    mix(x, tone(140, 0.12, 0.4, tau=0.03), at=0.14)
    return finish(x, -7)


def sfx_pad_chime():
    x = bell(1760, 1.0, 0.8, tau=0.5, partials=((1.0, 1.0), (2.0, 0.25), (3.0, 0.1)))
    return finish(x, -10)


def sfx_core_shutdown():
    rng = random.Random(81)
    sec = 4.2
    x = buf(sec)
    n = int(3.6 * SR)
    phase = 0.0
    wind = []
    for i in range(n):
        t = i / SR
        f = 40 + 180 * math.exp(-t / 0.9)                 # 220Hz → 40Hz 로 처짐
        phase += TAU * f / SR
        e = min(1.0, t / 0.05) * max(0.0, 1.0 - t / 3.6) ** 1.5
        s = math.sin(phase) + 0.5 * math.sin(phase * 2.01) + 0.3 * math.sin(phase * 3.02)
        wind.append(s * e * 0.6)
    mix(x, wind)
    mix(x, noise(3.0, 0.3, tau=1.2, attack=0.02, lp=700, rng=rng))           # 기계가 멎는 바람
    mix(x, noise(0.04, 0.8, tau=0.006, hp=2500, rng=rng))                    # 차단기 딸깍
    mix(x, tone(60, 0.8, 0.9, tau=0.25, freq_end=35), at=0.0)                # 쿵
    mix(x, bell(587.33, 1.6, 0.3, tau=0.8), at=3.3)                          # 마지막 '레' (모티프가 풀림)
    return finish(x, -2)


SOUNDS = {
    "SFX_Click": sfx_click,
    "SFX_Beep": sfx_beep,
    "SFX_Accept": sfx_accept,
    "SFX_Deny": sfx_deny,
    "SFX_Success": sfx_success,
    "SFX_LeverClunk": sfx_lever_clunk,
    "SFX_DoorOpen": sfx_door_open,
    "SFX_ElevatorDing": sfx_elevator_ding,
    "SFX_PlateThud": sfx_plate_thud,
    "SFX_Dispense": sfx_dispense,
    "SFX_Snap": sfx_snap,
    "SFX_RingTurn": sfx_ring_turn,
    "SFX_PadChime": sfx_pad_chime,
    "SFX_CoreShutdown": sfx_core_shutdown,
}


def write_wav(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))


def main():
    project = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(project, "Saved", "SFX")
    os.makedirs(out_dir, exist_ok=True)
    for name, fn in SOUNDS.items():
        samples = fn()
        path = os.path.join(out_dir, name + ".wav")
        write_wav(path, samples)
        print("%-18s %.2f초  %s" % (name, len(samples) / SR, path))


if __name__ == "__main__":
    main()
