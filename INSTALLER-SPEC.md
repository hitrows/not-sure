# Installer specification — Not Sure beta

Goal: one `.pkg` a friend can double-click that installs AU, VST3 and the
presets. Version **0.8.0**.

Do not use any `CMakeLists.txt` handed over in chat — that copy is stale
(0.6.0, before the preset generator and the 0.7.x optimisations). Edit the one
in the project.

---

## Part 1 — formats

Add VST3 back alongside AU. Keep Standalone in the build: it is how we audition
changes without a DAW rescan. It just does not go into the installer.

```cmake
FORMATS AU VST3 Standalone
```

Then verify the VST3 actually works — it has never been built or tested on this
project. Specifically:

- it loads in a VST3 host and the editor draws correctly
- parameters automate and the state saves and recalls
- bypass works (VST3 handles bypass differently from AU)
- latency reporting is right when Quality changes

If the VST3 turns out to be broken in a way that is not a quick fix, tell me
rather than shipping it — a beta that half works is worse than an AU-only beta.

Bump the project version to 0.8.0.

---

## Part 2 — the package

Build a component package per payload, then combine. Use `pkgbuild` and
`productbuild`; do not hand-roll a shell script installer.

Payloads and destinations:

| Payload | Destination |
| --- | --- |
| `Not Sure.component` | `/Library/Audio/Plug-Ins/Components/` |
| `Not Sure.vst3` | `/Library/Audio/Plug-Ins/VST3/` |
| the nine `.aupreset` files, with their folder structure | `~/Library/Audio/Presets/Hitrows/Not Sure/` |

Note the split: plugins go to the **system** location so every user account
sees them, presets go to the **user** location because that is the only place
Logic looks for `.aupreset`. That means the presets component needs
`--install-location` pointing at the home directory, or a postinstall script
that copies them for the installing user. Pick whichever you can verify
actually works — this is the part most likely to silently fail.

Do NOT install the Standalone app.

Add a `distribution.xml` with:

- title "Not Sure 0.8.0"
- a readme pane showing `README-BETA.md` converted to RTF or plain text
- host requirement: macOS 10.15 or later
- no license pane, no destination selection — one path only

Output: `NotSure-0.8.0.pkg` in a `dist/` folder. Add `dist/` to `.gitignore`.

Wrap the whole thing in `tools/build-installer.sh` so it is one command and
repeatable. The script should build Release, not Debug.

---

## Part 3 — signing

The plugin is currently signed with a local certificate that is only valid on
this machine. On anyone else's Mac, Gatekeeper will block it.

Right now we are shipping **unsigned**, and the README tells testers how to get
around it. So:

- make sure the build does not fail without a signing identity
- the resulting `.pkg` will be unsigned, which is expected
- do not add ad-hoc signing thinking it helps; it does not travel

If a Developer ID is bought later, the script should be easy to extend:
`codesign` each bundle with "Developer ID Application", `productsign` the pkg
with "Developer ID Installer", then `notarytool submit --wait` and `stapler
staple`. Leave a commented-out block in the script showing this, so the path is
obvious later.

---

## Part 4 — check it on a clean machine

The one thing that cannot be verified here: whether it installs somewhere other
than this Mac. Before sending it out, at minimum:

1. Delete the installed plugins and presets from this machine
2. Run the pkg
3. Confirm both formats appear and the presets show up in Logic's Settings menu
4. Confirm the plugin still validates: `auval -v aufx Nsur Htrw`

A pkg that only works because the files were already there is the classic way
this goes wrong.

---

## Acceptance

1. `tools/build-installer.sh` produces `dist/NotSure-0.8.0.pkg` from a clean
   checkout in one command.
2. Installing on a machine with no prior install puts AU, VST3 and nine presets
   in the right places.
3. No Standalone app is installed.
4. Presets appear in Logic's Settings menu, grouped into their folders.
5. VST3 loads, draws, automates and saves state in a VST3 host.
6. `auval -v aufx Nsur Htrw` still passes.
