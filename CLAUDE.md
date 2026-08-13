# Not Sure — project context

Audio plugin by Hitrows. An aggressive feedback limiter with a
charge-dependent release. Native AU on macOS (VST3 and Standalone built
alongside).

The name is a pun on Shure, whose Level-Loc inspired the behaviour. This is
not a clone and must never become one.

---

## Working rules

**Always propose before acting.** Present the options, state a recommendation,
ask "Делаем?" and wait for an explicit yes. This applies to writing code,
choosing names, editing files, and running commands. Do not skip this because
a step looks obvious or was already discussed.

The user writes in Russian. Respond in Russian. All plugin-facing strings,
code, comments and identifiers are in English.

The user is not an experienced C++ developer. Comment DSP and JUCE-specific
code densely; do not explain basic C++.

---

## What we are building and why

The Shure M62 "Level-Loc" (1960s) was a brick-wall limiter for public address
systems — designed to *minimise* artefacts. Engineers later discovered it
destroyed drums beautifully. Soundtoys built Devil-Loc on it; Korneff Audio
released an official Shure-approved emulation in 2025.

We are not competing with either. We take the same class of behaviour —
fixed low threshold, slow attack, charge-storage release — and build our own
character with controls the hardware never had.

### Measured behaviour of the original (from research, for reference only)

| Property | Value |
| --- | --- |
| Attack | fixed 1.3 ms — slow, transients survive |
| Release, normal | 0.85 s (fast) to 1.7 s (slow) |
| Release, saturated | 11 s (fast) to 22 s (slow) |
| Darkness filter | down to ~1500 Hz, gentle slope, post-distortion |
| Side effect | slight low-frequency thinning |

**The key mechanism.** Release stretches roughly 13x depending on how deeply
the detector capacitor was charged. This is not conventional program-dependent
release — it is charge storage. Combined with the slow 1.3 ms attack it
produces the entire effect: the transient passes through untouched, the
limiter clamps the tail, and deep material holds the envelope open for tens of
seconds. That is what "inflates the room" — a quiet tail lifted 20–30 dB while
the hit stays put.

---

## Signal chain

```
in → crush (drive) → feedback limiter → [OS] crunch waveshaper [/OS]
   → darkness (tilt) → mix (parallel) → trim → out
```

Design decisions, with reasons:

- **Feedback detector, not feedforward.** Gain reduction is derived from the
  output and fed back. This gives the soft grab and natural program dependence
  that feedforward has to fake.
- **Fixed low threshold (~-30 dBFS), effectively infinite ratio.** There is no
  threshold control. Crush drives into it — that is the whole interface idea.
- **Sag drives release length via charge depth**, not a time knob. At 0 the
  detector behaves like an ordinary limiter (~200 ms). At 10 it reaches the
  20-second tar pit. This is our parameter; the hardware had only a two-way
  switch.
- **Asymmetric waveshaper for Crunch.** DC offset before `tanh`, 10 Hz highpass
  after to remove it. Produces even harmonics — fat, not fuzzy. Crunch controls
  both the offset depth and the pre-shaper gain.
- **Darkness is a tilt filter, not a plain lowpass.** A lowpass at 1.5 kHz
  removes energy; a tilt moves it. Also compensates the low-frequency thinning,
  which is an artefact rather than a feature.
- **Dry path must be delay-compensated** to match wet-path latency, or Mix at
  50% will comb-filter the top end.

---

## Parameters

IDs live in `Source/Parameters.h`. **They are permanent** — every string is
written into saved sessions and preset files. Renaming one breaks recall
silently for every existing project.

| ID | Name | Range | Purpose |
| --- | --- | --- | --- |
| `crush` | Crush | 0–10 | drive into the limiter |
| `crunch` | Crunch | 0–10 | post-limiter gain and distortion |
| `sag` | Sag | 0–10 | how far charge depth stretches release |
| `darkness` | Darkness | 0–10 | post-distortion tilt |
| `mix` | Mix | 0–100 % | parallel dry/wet |
| `trim` | Trim | ±24 dB | manual output |
| `schp` | SC highpass | 20–300 Hz | detector only; hardware had none |
| `autogain` | Auto gain | bool | compensates Crush and Crunch |
| `attack` | Attack | 0.3 / 1.3 / 4.0 ms | 1.3 ms matches the hardware |
| `stereo` | Stereo | Linked / Unlinked / Mid-Side | M/S crushes room, keeps centre |
| `oversampling` | Quality | 1x / 2x / 4x | 1x is deliberately dirty |

Auto gain exists because the most common complaint about this class of plugin
is that every knob changes loudness, making A/B comparison useless.

---

## Performance requirements

The user explicitly asked for a plugin that is not heavy. Budget: 1–2% of one
core per instance at 48 kHz with 4x oversampling.

- **Denormals.** `ScopedNoDenormals` at the top of `processBlock`, plus a tiny
  DC offset into the detector. With 20-second releases the envelope tail *will*
  reach subnormal floats, and CPU load explodes on silence. This is not
  theoretical.
- **Oversample the waveshaper only**, not the whole chain. Detector, filters
  and mix stay at base rate — roughly 3x cheaper than oversampling everything.
- **UI repaints locally.** Meter timer at 25 Hz, not 60. `repaint()` on the
  meter rectangle only. Static background via `setBufferedToImage`. Naive
  full-editor repaint costs 5–15% of a core and is what gives plugins their bad
  reputation.
- **Parameters read once per block.** Attack/release coefficients recompute
  `exp` only when a knob actually moved. No transcendental functions in the
  sample loop.
- Report latency correctly via `setLatencySamples` once oversampling is in.

---

## Presets (not implemented yet)

Header strip: name centred, arrows either side, click opens a categorised
popup. `juce::PopupMenu` with `addSectionHeader`, not `ComboBox`.

- Factory presets embedded via BinaryData, undeletable
- User presets in `~/Library/Audio/Presets/Hitrows/Not Sure/`
- Subfolders on disk become menu categories — no code needed per category
- Schema version in every file, or a future parameter change breaks old presets
- Dot next to the name when the loaded preset has been modified
- Arrows step through the flat list, ignoring category boundaries
- All parameters ramp over 20–30 ms on preset load, or it clicks
- Scan the directory when the editor opens, never per click, never on the audio
  thread
- Do **not** expose these as AU factory presets — `getNumPrograms` stays at 1

---

## Build

```sh
/opt/homebrew/bin/cmake -B build -G Xcode
/opt/homebrew/bin/cmake --build build --config Debug
auval -v aufx Nsur Htrw
```

Environment facts learned the hard way:

- `cmake` is **not** on PATH. Call it as `/opt/homebrew/bin/cmake`.
- Xcode 26.6 must be the active toolchain, not Command Line Tools
  (`sudo xcode-select -s /Applications/Xcode.app`).
- **The project must live outside iCloud and `~/Documents`.** iCloud stamps
  `com.apple.FinderInfo` on files and `codesign` then fails with "resource
  fork, Finder information… not allowed". Current location:
  `/Users/hitrows/Developer/Not Sure`. Keep `build/` there too.

`COPY_PLUGIN_AFTER_BUILD` installs to `~/Library/Audio/Plug-Ins/`.
The `NotSure_Standalone` target is the fastest way to hear a change in a real
host — no DAW rescan, no plugin cache. It is built only during the tuning
phase; drop it before release if AU-only is wanted.

If the AU does not appear: `killall -9 AudioComponentRegistrar`, and quit Logic
fully — it only rescans at launch.

### Offline renderer — the main tuning loop

```sh
/opt/homebrew/bin/cmake --build build --config Debug --target notsure-render
./build/tools/notsure-render loop.wav out.wav --crush 7 --sag 9
./build/tools/notsure-render loop.wav sag.wav --crush 7 --sweep sag 0 10 6
```

Links `LimiterCore` only — no JUCE — so it builds in seconds. Sweep mode writes
one file per value with a numeric suffix. Reach for this before opening a DAW:
rendering the same loop across ten Sag values and listening back to back is how
character actually gets tuned.

`tools/WavFile.h` is a dependency-free reader/writer: 16/24/32-bit PCM and
32-bit float, mono or stereo, always writes 32-bit float.

**Identity is frozen.** `PLUGIN_CODE` = `Nsur`, `MANUFACTURER_CODE` = `Htrw`,
`BUNDLE_ID` = `com.hitrows.notsure`. Changing any of these makes hosts treat it
as a different plugin and drops existing sessions. JUCE is pinned to tag
8.0.15 — never track a branch.

---

## Status

Builds clean, `AU VALIDATION SUCCEEDED`, all eleven parameters declared,
state save/restore working, `ScopedNoDenormals` in place.

Editor is still `GenericAudioProcessorEditor` — deliberately temporary, so
every parameter is reachable while tuning DSP without committing to a UI.

### DSP stage 1 of 4 — done

`Source/dsp/LimiterCore.{h,cpp}`. Feedback detector, gain computer, attack, and
the charge-dependent release behind Sag. No JUCE includes anywhere in it. The
processor is a thin wrapper that marshals APVTS values into a `Params` struct.

Live parameters: `crush`, `sag`, `attack`, `mix`, `trim`.
Declared but not yet wired: `crunch`, `darkness`, `schp`, `autogain`,
`stereo`, `oversampling`.

Measured on a synthetic drum loop, hits every 0.5 s, `crush 7`:

| Sag | tail at +0.35 s |
| --- | --- |
| 0 | −3.9 dB |
| 5 | −15.9 dB |
| 10 | −20.4 dB |

Long release holds the gain cell down and sucks the tail out; short release
lets it recover and lifts the room. This matches the source material, where
maximum settings collapse the signal to silence between hits. Crush is
monotonic too: tail rises −21 → −5.5 dB across its range.

Cost: ~250x realtime, stereo at 44.1 kHz. Roughly 0.4% of a core.

### ⚠️ Open issue — the tanh in LimiterCore is a placeholder

First test run peaked at **+28 dBFS**. A 1.3 ms attack lets transients through
before the detector reacts; Crush drives them and makeup lifts them further.

The hardware does not have this problem because its output transistor stage
clips the overshoot. **That stage is Crunch.** It is therefore not decoration
to add later — it is load-bearing gain structure. A `std::tanh` currently sits
in its place at the output of `process()` and holds peaks at 0 dBFS.

Do not treat that tanh as finished, and do not tune character on top of it for
long. Stage 2 replaces it with the real asymmetric waveshaper.

### Next steps

1. **Stage 2** — Crunch waveshaper (DC offset before `tanh`, 10 Hz highpass
   after) with oversampling around it only. Replaces the placeholder. Report
   latency via `setLatencySamples` and delay-compensate the dry path.
2. **Stage 3** — Darkness tilt, auto gain, wired into the chain.
3. **Stage 4** — sidechain highpass, stereo/mid-side modes.
4. Tuning by ear against real loops via `notsure-render`.
5. Custom UI and preset browser.
