# Windows build specification — Not Sure

Target version: **0.9.1**.

Build a Windows VST3 on GitHub Actions without owning a Windows machine to
build on. The user has a Windows 10 laptop for **testing**, which is the part
that cannot be automated.

---

## Why CI rather than cross-compiling

JUCE on Windows expects MSVC, which only exists on Windows. MinGW is
unofficial and patchy. GitHub Actions gives public repositories free
`windows-latest` runners with MSVC and CMake already installed, so the build
happens on a real Windows machine that we never have to own.

---

## Part 1 — the workflow

`.github/workflows/windows.yml`, triggered on push to `main` and manually via
`workflow_dispatch`.

Steps: checkout, configure with CMake, build Release, upload the VST3 as a
build artifact.

Two things worth getting right:

- **Cache the JUCE clone.** `FetchContent` pulls JUCE on every run otherwise,
  which is minutes per build. Key the cache on the pinned tag, 8.0.15.
- **Build Release, not Debug.** A Debug VST3 is slow enough that a tester will
  report performance problems that do not exist.

Formats on Windows: **VST3 only.** AU does not exist off macOS. Standalone is
not needed — it exists for our own auditioning on the Mac.

Make sure `FORMATS` is conditional rather than hard-coded, so the same
`CMakeLists.txt` serves both platforms.

---

## Part 2 — what will actually break

None of the code below has ever run on Windows. Expect to fix things rather
than expecting it to work.

### Paths

Everything under `~/Library` is macOS-only. The update checker's settings file
and the licence file both live there today.

Use `juce::File::getSpecialLocation (userApplicationDataDirectory)` and let
JUCE resolve it — `~/Library/Application Support` on macOS, `%APPDATA%` on
Windows. If either path is built by string concatenation anywhere, that is the
bug.

Presets have the same problem and a worse one — see below.

### Presets

`tools/make-presets.mm` is Objective-C++ against AudioToolbox. It cannot run on
Windows and `.aupreset` is a macOS format that Windows hosts do not read.

VST3 uses `.vstpreset`, a different container entirely. That is real work and
it is **not part of 0.9.1**. For this release the Windows build ships without
factory presets, and the README says so plainly. Do not fake it.

### Installer

`build-installer.sh` is bash plus `pkgbuild`. Windows needs Inno Setup or WiX.

For 0.9.1, **ship a zip** with the `.vst3` and a short text file saying to drop
it in `C:\Program Files\Common Files\VST3`. A proper installer can come later;
a zip is honest and takes no time.

### Code signing

Windows needs its own certificate, unrelated to Apple's, and it is not cheap.
Unsigned means SmartScreen will warn, much like Gatekeeper. The README already
explains the macOS side; add the Windows equivalent.

### Smaller things likely to bite

- Line endings — add a `.gitattributes` if the build complains
- `#include` paths with the wrong case: macOS is case-insensitive, Windows is
  not, and this fails only on Windows
- MSVC warnings that Clang does not emit, especially around `-Wfloat-equal`
  equivalents and narrowing conversions
- `std::min`/`std::max` colliding with Windows macros — define `NOMINMAX` if it
  comes up

---

## Part 3 — testing, which is the actual point

A build that compiles is not a build that works. On the Windows 10 laptop:

1. The VST3 loads in a host and the editor draws — the panel is a large image
   composited with overlays, and this has never been exercised off macOS
2. All six knobs move their parameters and the pointer draws correctly
3. The three switch tiles land in the right place
4. Bypass and the lamp work
5. State saves and reloads with the project
6. Automation works
7. The update check runs, or fails silently offline
8. `pluginval --strictness-level 10` passes

Run pluginval on Windows too — it exists there and catches host-contract
problems that a manual test will not.

---

## Acceptance

1. A push to `main` produces a downloadable Windows VST3 artifact.
2. `CMakeLists.txt` builds correctly on both platforms from the same file.
3. No path in the source assumes `~/Library`.
4. The plugin loads and passes pluginval on the Windows laptop.
5. The README states clearly that the Windows build has no factory presets yet
   and is unsigned.
