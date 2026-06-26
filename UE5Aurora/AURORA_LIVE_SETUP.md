# Aurora — Live Reactivity Setup (sound + phone position)

On-site reference for the wall-projection install. Everything below is configured
on the **DataMixer** component of the `AuroraBorealisActor` in the level, unless
noted. Turn on `Aurora|Debug → bShowDebugHUD` while setting up — it prints live
`AUDIO:` and `POS:` lines (cyan = data flowing, red = nothing arriving yet).

The piece degrades gracefully: with no sound and no phone connected it still runs
its vivid autonomous baseline (slow breathing + hue drift). Sound and motion are
layered on **additively**, so silence never dims it.

---

## 1. Sound (local mic / line-in)

Real-time spectral analysis of the default audio **input** device. Split into
bass / mid / treble + beat detection.

- `Aurora|Audio → bUseAudio = true`
- `bUseMicrophone = true`
- Pick the input device in **Windows → Sound → Input** (set your interface/mic as
  default). UE captures whatever Windows' default input is.
- **Recommended (reliable + silent):** create a `SoundSubmix` asset, open it, set
  its **Output Volume to 0.0**, and assign it to `AudioAnalysisSubmix`. This both
  guarantees the analyzer registers correctly and ensures the mic is never
  monitored through speakers (no feedback). If you leave `AudioAnalysisSubmix`
  empty, one is created at runtime as a fallback — watch the log/HUD; if `AUDIO:`
  stays at 0, assign an asset.

Tuning:
- `AudioInputGain` (default 12) — raise for a quiet room/mic, lower if it's pinned
  bright. Watch the HUD `AUDIO:` numbers hover around 0.2–0.7 on normal material.
- `BeatSensitivity` (default 1.4) — lower = more beats fire, higher = only strong
  hits. Beats trigger bright flares (reuses the substorm flare path).
- `AudioNumBands`, `AudioMinFrequency`/`AudioMaxFrequency` — analysis resolution
  and range; defaults (24 bands, 40–14000 Hz) are fine.

What it drives: bass → emissive + intensity + luminance breathing; mid → fold
turbulence (sway amplitude); treble → vertical-ray sparkle; beats → flares +
highlight particle bursts.

---

## 2. Phone position (GyrOSC → Maestra → entity state)

No OSC code runs in Unreal. The phone's GyrOSC app sends OSC to **Maestra's OSC
mapper**, which writes x/y/z into a Maestra entity's state. The app reads those
state keys live over the Maestra WebSocket.

- `Aurora|Maestra → MaestraApiUrl` / `MaestraWebSocketUrl` → point at Maestra.
  (If you only want position, you don't need `bUseMaestra`/pots on — `bUsePosition`
  brings up its own client connection.)
- `Aurora|Position → bUsePosition = true`
- `PositionEntitySlug` → the entity slug your OSC mapper writes to. (Leave empty to
  reuse `MaestraEntitySlug`.)
- `PosXKey` / `PosYKey` / `PosZKey` → **must match the state-key names your OSC
  mapper produces** (defaults `x` / `y` / `z`).
- `PosInputMin` / `PosInputMax` → the raw range your sender emits (GyrOSC
  accel/attitude is typically about −1..1). Tune so the HUD `POS:` values sweep the
  full 0–1 as you move the phone.
- `PositionSmoothing` (snappy vs floaty), `MotionSensitivity` (how much a shake
  erupts the aurora).

Mapping (intuitive to demo with the phone):
- **X axis → FocusX**: a bright hotspot slides horizontally across the wall.
- **Y axis → FocusEnergy**: how bright/concentrated that hotspot is, plus overall
  intensity + curtain height.
- **Motion (rate of change of x/y/z) → MotionEnergy**: shaking/flicking the phone
  adds turbulence, wave speed, and flares.

> GyrOSC sends several OSC streams (`/gyrosc/accel`, `/gyrosc/gyro`, `/gyrosc/rrate`,
> etc.). In Maestra's OSC mapper, route the three components of whichever stream
> feels best (accel for tilt, gyro for rotation) into three entity keys, then put
> those key names in `PosXKey/PosYKey/PosZKey`.

---

## 3. Look / tuning knobs (on the AuroraBorealisActor)

`Aurora|Reactivity`:
- `HotspotStrength` (default 1.8) — peak brightness of the position hotspot; 0
  disables the moving spot.
- `HotspotWidth` (default 0.13) — tighter = more of a spotlight, wider = a soft glow.
- `BassEmissiveStrength` (default 3.5) — how hard the bass punches the glow.
- `TrebleShimmerStrength` (default 0.9) — how much treble glitters the rays.

---

## 4. Quick on-site checklist

1. Set Windows default **input** device to the mic/interface.
2. DataMixer: `bUseAudio`+`bUseMicrophone` on; assign a 0-output `SoundSubmix` to
   `AudioAnalysisSubmix`.
3. DataMixer: set Maestra URLs; `bUsePosition` on; set `PositionEntitySlug` and the
   three Pos*Key names to match the OSC mapper.
4. Start GyrOSC on the phone → confirm Maestra entity state shows x/y/z updating.
5. Play in the level with `bShowDebugHUD` on → confirm `AUDIO:` and `POS:` lines are
   cyan and moving. Tune `AudioInputGain` and `PosInputMin/Max`.
6. Turn `bShowDebugHUD` off for the show.
