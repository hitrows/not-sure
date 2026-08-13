# Not Sure

An aggressive feedback limiter with a charge-dependent release.
By Hitrows.

Skeleton stage: hosts, saves state and validates as an Audio Unit. No DSP yet.

## Requirements

- Xcode with Command Line Tools (`xcode-select --install`)
- CMake 3.22+ (`brew install cmake`)
- Git

## Build

```sh
cmake -B build -G Xcode
cmake --build build --config Debug
```

First configure clones JUCE into `build/_deps` and takes a few minutes.
Everything after that is incremental.

`COPY_PLUGIN_AFTER_BUILD` installs the results automatically:

- `~/Library/Audio/Plug-Ins/Components/Not Sure.component`
- `~/Library/Audio/Plug-Ins/VST3/Not Sure.vst3`

## Validate the Audio Unit

```sh
auval -a | grep -i "not sure"      # is it registered at all
auval -v aufx Nsur Htrw            # full validation
```

A clean run ends with `AU VALIDATION SUCCEEDED`.

If it does not appear, macOS is caching the old component list:

```sh
killall -9 AudioComponentRegistrar
```

Logic and GarageBand only rescan on launch, so quit them fully first.

## Standalone

The `NotSure_Standalone` target runs without a host and is the fastest way to
hear a change - no DAW rescan, no plugin cache.

## Presets

User presets live in:

```
~/Library/Audio/Presets/Hitrows/Not Sure/
```

Subfolders there become categories in the preset menu. Not implemented yet.

## Parameters

| ID | Name | Range | Notes |
|---|---|---|---|
| `crush` | Crush | 0–10 | drive into the limiter |
| `crunch` | Crunch | 0–10 | post-limiter gain and distortion |
| `sag` | Sag | 0–10 | how far charge depth stretches release |
| `darkness` | Darkness | 0–10 | post-distortion tilt |
| `mix` | Mix | 0–100 % | parallel dry/wet |
| `trim` | Trim | ±24 dB | manual output |
| `autogain` | Auto gain | on/off | compensates Crush and Crunch |
| `attack` | Attack | 0.3 / 1.3 / 4.0 ms | 1.3 ms matches the hardware |
| `oversampling` | Quality | 1x / 2x / 4x | around the waveshaper only |
| `bypass` | Bypass | on/off | host bypass button maps here too |

Parameter ID strings are permanent - they are written into every session and
preset file. Renaming one breaks recall silently.

## Identity

Set in the block at the top of `CMakeLists.txt`. `PLUGIN_CODE` (`Nsur`) and
`MANUFACTURER_CODE` (`Htrw`) must each stay exactly four characters with at
least one uppercase, or the AU will not load. Changing either one makes the
host treat it as a different plugin and drops existing sessions.

## Next steps

1. DSP core as a framework-free class, testable against wav files
2. Character tuning by ear
3. Custom UI and preset browser
