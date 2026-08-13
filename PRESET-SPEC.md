# Preset specification — Not Sure

Factory presets exposed through the host's own preset menu, so Logic's
Setting dropdown, Save As, Copy/Paste and user folders all work the way they do
for stock plugins. No preset UI of our own — there is no room on the panel for
one, and the native menu is better anyway.

---

## What to implement

### Program interface

`AudioProcessor` currently returns 1 program. Replace with:

```cpp
int getNumPrograms() override;              // presets.size()
int getCurrentProgram() override;           // index of last loaded
void setCurrentProgram (int index) override;
const juce::String getProgramName (int index) override;
void changeProgramName (int, const juce::String&) override {}   // no rename
```

`setCurrentProgram` writes each stored value into the APVTS parameter, using
`setValueNotifyingHost` on the normalised value so the host and the editor both
follow. Store the index and keep returning it from `getCurrentProgram`.

### AU factory presets

Program methods alone are not enough — without this Logic shows only
"Default". Override:

```cpp
void getAUFactoryPresets (...)   // or the JUCE 8 equivalent
```

Check the JUCE 8.0.15 API for the current shape of this — it has moved between
versions. If the wrapper exposes it via `getNumPrograms` alone in this version,
verify by loading in Logic rather than assuming.

### Categories

Logic groups presets by folder structure in the name. Use a forward slash:

```
"Drums/Room Crush"
"Drums/Snare Fatten"
"Bass/Weight"
```

Verify this actually groups in Logic. If it does not, fall back to flat names
with a prefix ("Drums — Room Crush") and tell me rather than inventing a
different scheme.

### State

`getStateInformation` already saves everything. User presets via Save As
therefore work with no extra code — do not add a parallel save path.

---

## The presets

Values are `crush, crunch, sag, darkness, mix, trim, attack, quality`.
`autogain` is on for all of them; `bypass` is always off.

Attack is an index: 0 = 0.3 ms, 1 = 1.3 ms, 2 = 4.0 ms.
Quality is an index: 0 = 1x, 1 = 2x, 2 = 4x.

| Name | crush | crunch | sag | darkness | mix | trim | attack | quality |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Default | 0 | 0 | 0 | 0 | 100 | 0 | 1 | 2 |
| Drums/Room Crush | 3.5 | 2.0 | 8.0 | 3.0 | 45 | 0 | 1 | 2 |
| Drums/Snare Fatten | 5.0 | 4.0 | 4.0 | 2.0 | 70 | 0 | 1 | 2 |
| Drums/Bus Glue | 2.0 | 1.5 | 3.0 | 1.5 | 35 | 0 | 2 | 2 |
| Drums/Loop Destroy | 8.0 | 7.0 | 9.0 | 5.0 | 100 | 0 | 0 | 0 |
| Bass/Weight | 4.5 | 3.0 | 2.0 | 4.5 | 60 | 0 | 2 | 2 |
| Vocal/Grit | 6.0 | 5.5 | 3.5 | 3.0 | 28 | 0 | 1 | 2 |
| Keys/Synth Thicken | 7.0 | 2.5 | 5.0 | 5.5 | 55 | 0 | 1 | 2 |

Default must be index 0 and must be transparent — every dial at zero.

### Where these came from

Each was rendered through `notsure-render` against a drum loop and checked, so
the numbers do something rather than merely being plausible:

| Preset | ΔRMS | crest factor |
| --- | --- | --- |
| source | — | 23.8 dB |
| Bus Glue | −0.5 dB | 22.7 dB |
| Room Crush | −0.7 dB | 20.9 dB |
| Vocal Grit | −0.6 dB | 21.9 dB |
| Synth Thicken | −1.7 dB | 19.6 dB |
| Bass Weight | −0.8 dB | 18.8 dB |
| Snare Fatten | −0.4 dB | 16.9 dB |
| Loop Destroy | −2.9 dB | 9.7 dB |

Loudness stays within about a dB across the whole set — that is auto gain
doing its job — while the crest factor drops steadily from gentle to
destroyed. That progression is the point: each preset squashes harder than the
last rather than just being differently named.

`Loop Destroy` deliberately runs quality at 1x. The aliasing is the character,
not an oversight — do not "fix" it to 4x.

These are starting points to be tuned by ear. Keep them in one table in the
source so they are easy to edit; do not scatter the numbers.

---

## Acceptance

1. Builds clean, `auval -v aufx Nsur Htrw` passes.
2. Logic's Setting dropdown lists all eight, grouped by category if the slash
   naming works.
3. Selecting one moves the on-screen knobs and switches, and changes the sound.
4. Default is transparent — a loop through it is indistinguishable from bypass.
5. Save As stores a user preset and it reloads correctly.
6. Loading a preset then saving the Logic project and reopening it recalls the
   right values.
7. Automation still works after a preset change — parameters must move via
   `setValueNotifyingHost`, not by writing raw values behind the host's back.
