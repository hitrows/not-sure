# Not Sure — project context

Audio plugin by Hitrows. An aggressive feedback limiter with a
charge-dependent release. AU and VST3 on macOS, plus a Standalone build used
only for auditioning changes without a DAW rescan.

The name is a pun on Shure, whose Level-Loc inspired the behaviour. This is not
a clone and must never become one.

**Path:** `/Users/hitrows/Developer/Not Sure`
**Repo:** https://github.com/hitrows/not-sure (public)

---

## Working rules

**Always propose before acting.** Present options, state a recommendation, ask
"Делаем?" and wait for an explicit yes. This covers writing code, choosing
names, editing files, running commands. Do not skip it because a step looks
obvious or was discussed earlier.

The user writes in Russian. **Respond in Russian.** All code, comments,
identifiers and plugin-facing strings are in English.

The user is not an experienced C++ developer. Comment DSP and JUCE-specific
code densely; do not explain basic C++.

**Measure, do not assume.** Every claim about behaviour in this project has
been settled by measurement, and several confident guesses turned out wrong.
Acceptance criteria in the specs are mechanical on purpose. "Correct by
construction" is not a result.

---

## Where things stand

Version **0.8.1** is built and in testers' hands as an unsigned `.pkg`.

Note a deliberate desync: `version.json` in the repo advertises **0.8.2** so
the update notice could be tested. The next release is planned as **0.8.3**,
which brings both back into line.

Two specs are written and not yet implemented:

- `OVERSAMPLING-SPEC.md` — target 0.8.3. Fixed latency, sample-rate cap on the
  oversampling factor, maximum quality during offline render.
- `STATE-VERSION-SPEC.md` — target 0.8.4. Schema version stamped into saved
  state, hardened state loading, version tooltip on the bottom-right screw.

Read the spec before touching either area. They contain measured numbers and
explain traps that are not obvious from the code.

---

## Environment (learned the hard way)

- Project and `build/` must live **outside iCloud and `~/Documents`**. iCloud
  stamps `com.apple.FinderInfo` on files and `codesign` then fails with
  "resource fork, Finder information… not allowed".
- `cmake` is not on PATH. Call `/opt/homebrew/bin/cmake`.
- Full Xcode must be the active toolchain, not Command Line Tools:
  `xcode-select -p` should print `/Applications/Xcode.app`.
- **CPU measurements only in Release.** In a Debug `-O0` build the library
  functions beat our approximations and the numbers are meaningless.
- Logic scans AU only at launch. After a rebuild:
  `killall -9 AudioComponentRegistrar`, then quit Logic fully with Cmd+Q.
- Names in `BinaryData` lose hyphens and underscores. `switch-center.png`
  becomes `switchcenter_png`. Name resource files accordingly.

### Frozen identity

`PLUGIN_CODE=Nsur`, `MANUFACTURER_CODE=Htrw`, `BUNDLE_ID=com.hitrows.notsure`,
JUCE pinned to tag **8.0.15**.

Parameter ID strings in `Source/Parameters.h` are permanent — every one is
written into saved sessions and `.aupreset` files. Renaming one breaks recall
silently for every existing project.

---

## Build and validate

```sh
/opt/homebrew/bin/cmake -B build -G Xcode
/opt/homebrew/bin/cmake --build build --config Debug
auval -v aufx Nsur Htrw
```

VST3: `pluginval --strictness-level 10 --validate <.vst3>`.

### The offline renderer — main tuning loop

```sh
./build/tools/notsure-render loop.wav out.wav --crush 7 --crunch 5 --sag 9
./build/tools/notsure-render loop.wav sag.wav --sweep sag 0 10 6
```

Links `LimiterCore` and `Oversampler` only — no JUCE — so it builds in seconds.
Sweep mode writes one file per value. Reach for this before opening a DAW:
rendering the same loop across ten values and listening back to back is how
character actually gets tuned, and it is also how null tests are run.

For CPU numbers build `--config Release --target notsure-render`.

---

## Architecture

```
in → crush (drive) → feedback limiter → [OS] crunch waveshaper [/OS]
   → soft ceiling → darkness (tilt) → mix → auto gain → trim → out
```

- `Source/dsp/LimiterCore.{h,cpp}` — all character. **No `#include <juce...>`
  anywhere in it.** That is what lets `notsure-render` hear exactly what the
  plugin hears. Keep it that way.
- `Source/dsp/Oversampler.{h,cpp}` — halfband polyphase, 2x per stage, cascaded
  for 4x. 49 taps, Kaiser beta 7.0, −72 dB stopband. Ring buffers, symmetry
  folded (24 → 12 multiplies).
- `Source/PluginProcessor.{h,cpp}` — thin wrapper, marshals APVTS into
  `LimiterCore::Params`.
- `Source/PluginEditor.{h,cpp}` — see `UI-SPEC.md`.
- `Source/Presets.h` — preset values, single table.
- `Source/UpdateChecker` — see `UPDATE-CHECK-SPEC.md`.

### Why it sounds the way it does

The Shure M62 "Level-Loc" (1960s) was a brick-wall limiter for public address,
designed to *minimise* artefacts. Engineers later found it destroyed drums
beautifully. Measured behaviour of the original:

| Property | Value |
| --- | --- |
| Attack | fixed 1.3 ms — slow, transients survive |
| Release, normal | 0.85 to 1.7 s |
| Release, saturated | 11 to 22 s |
| Tone filter | down to ~1500 Hz, gentle slope, post-distortion |

**The mechanism.** Release stretches roughly 13x depending on how deeply the
detector capacitor was charged — not conventional program dependence, but
charge storage. Combined with the slow attack it produces everything: the
transient passes untouched, the limiter clamps the tail, dense material holds
the envelope open for tens of seconds. A quiet tail lifted 20–30 dB while the
hit stays put is what "inflates the room".

### Load-bearing decisions

- **Feedback detector, not feedforward.** Gain reduction derives from the
  output and feeds back. Gives the soft grab and natural program dependence
  that feedforward has to fake.
- **Fixed threshold at −30 dBFS, effectively infinite ratio.** There is no
  threshold control; Crush drives into it. That is the whole interface idea.
- **Drive spans −42 to +10 dB**, so Crush 0 passes a full-scale signal down to
  exactly the threshold and nothing is limited. An earlier version started
  drive at unity, which left normal material 30 dB over the threshold and
  crushed hard at zero.
- **Makeup is tied to the threshold, with headroom — not to the drive floor.**
  0.8.x–0.9.1 set makeup to exactly minus the drive floor (+42 dB), which put
  the limiter's held output at +12 dBFS and left `softCeiling` doing the
  limiter's job as a brick-wall clipper — measured at up to ~12% of a render
  sitting in flat-topped clipping instead of being limited. Fixed in 0.9.2
  (`GAIN-FIX-SPEC.md`): `kMakeupHeadroomDb`, currently 10 dB, tuned against a
  measured render rather than assumed from the formula. Auto gain was
  refitted from scratch against the new levels — do not reuse pre-0.9.2
  auto-gain constants, they were fitted to the old, clipping behaviour.
- **Sag drives release length via charge depth.** At 0 it collapses to a 0.2 s
  release; at 10 fully charged it reaches 20 s.
- **Crunch is not decoration.** A 1.3 ms attack lets transients past the
  detector; without a bounded output stage peaks reached +28 dBFS. In the
  hardware the output transistor stage catches this. The shaper is normalised
  as `tanh(x*d)/d` so `d` controls curvature and not level — at Crunch 0 it is
  linear to about one percent.
- **Darkness is a tilt, not a lowpass.** A lowpass removes energy; a tilt moves
  it. Pivot 900 Hz, up to +4 dB low and −14 dB high.
- **Auto gain must be the last thing in the wet path.** Everything before it is
  nonlinear, so a calibration measured with auto gain off only transfers if the
  compensation is pure output scaling. Moving it earlier threw the calibration
  out by 7 dB. Fitted to a measured crush × crunch grid, residual 0.9 dB.
- **Dry path is delay-compensated** against the oversampler, or Mix below 100%
  comb-filters the top end.

---

## Parameters (10)

| ID | Name | Range |
| --- | --- | --- |
| `crush` | Crush | 0–10 |
| `crunch` | Crunch | 0–10 |
| `sag` | Sag | 0–10 |
| `darkness` | Darkness | 0–10 |
| `mix` | Mix | 0–100 % |
| `trim` | Trim | ±24 dB |
| `autogain` | Auto gain | bool |
| `attack` | Attack | 0.3 / 1.3 / 4.0 ms |
| `oversampling` | Quality | 1x / 2x / 4x |
| `bypass` | Bypass | bool, wired to `getBypassParameter()` |

All dials default to zero, so a fresh instance is transparent.

---

## Presets

`.aupreset` files on disk under
`~/Library/Audio/Presets/Hitrows/Not Sure/<category>/`, visible in Logic's
**Settings** menu. Nine of them: Default, Drums (four), Bass, Vocal (two), Keys.

**AU factory programs are switched off** (`getNumPrograms() == 0`) — that
removes the flat "AU Presets" menu. Logic does **not** parse "/" in a program
name into folders; real folders on disk are the only way.

Generated by `tools/make-presets.mm`, which takes the `ClassInfo` template from
the AU and substitutes a hand-built `jucePluginState`. It writes state directly
rather than going through `AudioUnitSetParameter`, because JUCE normalises AU
parameters 0..1 with skew and raw values break the middle of the range. Values
come from `Source/Presets.h`.

---

## Installer

`tools/build-installer.sh` produces `dist/NotSure-<version>.pkg` in one command,
Release, currently **unsigned**.

Plugins go to system `/Library/Audio/Plug-Ins/{Components,VST3}`; the nine
presets go to the **user** `~/Library/Audio/Presets/` via a postinstall script,
because a system-domain package cannot write to `$HOME`. Standalone is not
installed.

`README-BETA.md` tells testers how to get past Gatekeeper. A Developer ID
signing block sits commented out at the end of the script.

---

## Do not redo these

Each was tried, measured, and rejected. Re-attempting them wastes a session.

- **Self-updating.** The bundle is memory-mapped while loaded; overwriting it
  crashes the host. Installing needs an admin password. Logic sandboxes
  plugins. The plugin reports; a human installs.
- **Lookahead.** It would work, and it would destroy the character. The slow
  attack letting transients through is the entire point.
- **Latency as a way to save CPU.** Oversampling cost is sample count. Longer
  filters cost more CPU *and* more latency, not less.
- **Fast log/exp in the detector.** Measured, no gain on Apple Silicon — the
  detector runs at base rate and is not the bottleneck. Rolled back.
- **Measuring knob centres by thresholding dark pixels.** Cap shadows drag the
  centroid 20–40 px low; correcting by eye then overshoots the other way. Fit
  the chrome collar ring instead, or ask the user to mark the artwork.
- **Breaking the pointer line into segments** to fake wear. Reads as a dotted
  line at normal size.
- **VST2.** Steinberg closed the SDK in 2018 and terminated existing agreements
  in 2024. Not licensable at any price.
- **AAX.** Needs an Avid developer account, a Pro Tools Developer build and
  PACE signing — months of correspondence.
- **Stereo modes and the sidechain highpass.** Both existed and were removed
  deliberately. If mid/side ever returns it gets a new parameter ID; never
  revive `stereo` or `schp`, because old sessions would recall junk into them.

---

## Commercial plans

The user intends to sell from 1.0 and to add a Windows VST3 build (they have a
Windows 10 laptop for testing).

Two things that follow:

- **JUCE 8's free tier needs no splash screen** and allows commercial release
  up to $20k gross revenue. `JUCE_DISPLAY_SPLASH_SCREEN=0` is legitimate.
  Re-read the JUCE 8 EULA before launching sales.
- **DSP behaviour becomes frozen at 1.0.** Between 0.6 and 0.8 the meaning of
  the same knob positions changed several times — drive floor, auto gain
  calibration, shaper normalisation, filter slope. Fine while fixing mistakes;
  not fine once someone has mixed a record. From 1.0, changes must be additive
  or gated on the state schema version.
- The name is a deliberate pun on an active trademark holder who currently
  ships in this exact category. Raised with the user; their call.

---

## Specs in this repo

Read the relevant one before working in that area — they hold measured
coordinates, rejected alternatives and mechanical acceptance criteria.

| File | Covers |
| --- | --- |
| `UI-SPEC.md` | panel geometry, knob centres, switch tiles, pointer |
| `PRESET-SPEC.md` | preset values and how they were derived |
| `UPDATE-CHECK-SPEC.md` | version check, threading, privacy |
| `INSTALLER-SPEC.md` | pkg layout, signing path |
| `OVERSAMPLING-SPEC.md` | pending, 0.8.3 |
| `STATE-VERSION-SPEC.md` | pending, 0.8.4 |
| `README-BETA.md` | what testers are told |
