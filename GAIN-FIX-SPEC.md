# Gain structure fix — Not Sure

Target version: **0.9.2**. This is a **breaking sound change** and it is meant
to be. Do not ship it after 1.0.

---

## The finding

A side-by-side render against Devil-Loc Deluxe at matched settings (crush 6.9,
crunch 4.5, attack 1.3 ms) shows the plugin is **clipping into a square wave**,
not limiting.

Measured on the user's renders:

| | time spent above 90% of peak | crest factor |
| --- | --- | --- |
| Devil-Loc | 0.16 % | 8.94 dB |
| Not Sure | **11.52 %** | 6.74 dB |

Plotting the waveform makes it plain: Devil-Loc keeps the shape of each cycle,
ours has flat tops and flat bottoms. It is a near-perfect square.

Reproduced independently on a different loop through `notsure-render`: 8.94 % —
same regime.

## The cause

`kThresholdDb = -30`, and makeup is tied to the drive floor:

```cpp
makeupGain = dbToGain (-kDriveFloorDb);   // +42 dB
```

When the limiter is working it holds its output at the threshold, −30 dBFS.
Then +42 dB of makeup puts it at **+12 dBFS**, and `softCeiling` shaves off
those twelve decibels. The higher Crush goes, the worse it gets. Measured with
auto gain off:

| Crush | clipped |
| --- | --- |
| 0 | 0.00 % |
| 3 | 0.98 % |
| 6 | 12.45 % |
| 10 | 59.09 % |

So the limiter is not limiting — the ceiling is. A ceiling doing a limiter's
job is a clipper, and that is what everyone has been listening to.

Makeup was chosen so that Crush 0 passes at unity. That is correct for the
un-limited case and badly wrong for the limited one, which is every case that
matters.

## The fix

Tie makeup to the **threshold**, with headroom, rather than to the drive floor.
Verified by patching and re-rendering:

| Crush | now | makeup +30 | makeup +24 |
| --- | --- | --- | --- |
| 3 | 0.98 % | 0.00 % | 0.00 % |
| 6 | 12.45 % | 1.21 % | 0.10 % |
| 10 | 59.09 % | 12.65 % | 4.51 % |

At the user's exact settings, +24 dB gives **0.04 %** against Devil-Loc's
0.16 %.

Start from `makeup = -kThresholdDb - 6` and tune from there. Put it in one
named constant with a comment saying what it is for, so nobody re-ties it to
the drive floor later.

**Crush 0 will no longer be unity gain.** That is fine — auto gain exists to
handle exactly this, and it has to be recalibrated anyway.

## Recalibrate auto gain

The whole crush × crunch surface moves. Re-run the measurement the same way it
was done before: sweep the grid with auto gain off, fit, verify the residual is
about a decibel across the range. The existing fitted constants become invalid
the moment makeup changes — do not keep them.

---

## What this fix does not settle

Two comparisons in the A/B are **not** valid evidence and must not be used to
tune character:

**Crest factor across different sources.** The reproduction used a different
loop from the user's. Crest depends heavily on the source. Only the clipping
percentage is comparable between them, because it is a property of the
plugin's behaviour rather than the material.

**Sag has no counterpart.** It was at 10 — the maximum, up to a 20-second
release. Devil-Loc has a two-way release switch and nothing equivalent. "Same
settings" was true for Crush and Crunch only.

After the fix lands, get a **dry source file** from the user and render the
same material through Devil-Loc, the old build and the new one. That is the
only way to compare character honestly.

### Two differences worth investigating then, not now

With loudness normalised, against Devil-Loc:

- **−3.7 dB at 250–500 Hz.** The body of a drum. Some of this is the clipping
  and should improve on its own; check again afterwards before chasing it.
- **Envelope range 31.3 dB against Devil-Loc's 20.4 dB.** Devil-Loc lifts the
  quiet parts far more — that is the "inflating the room" behaviour the whole
  design is aimed at, and ours is not doing enough of it. This may be a Sag
  calibration question or a detector question. It is the most interesting
  remaining difference and it deserves its own investigation once the gain
  structure is honest.

---

## Acceptance

1. At crush 6.9 / crunch 4.5 / attack 1.3 ms, time above 90 % of peak is under
   1 %, measured on a rendered file rather than assumed.
2. The clipping table above is re-measured and every entry improves.
3. Auto gain holds output within about 1 dB across the crush × crunch grid,
   re-fitted — not the old constants.
4. Waveform at a loud moment has visible shape, not flat tops.
5. A null test against the previous build **will** fail. That is expected here;
   record how far it moved rather than treating it as a regression.
