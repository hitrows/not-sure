# UI specification — Not Sure

Everything below is measured from the artwork. Coordinates are in **design
space: 1792 x 592**. Scale them at runtime; never hard-code pixel values for a
particular window size.

---

## Part 1 — parameters (already done, do not redo)

`schp` removed, `bypass` added and wired to `getBypassParameter()`.
Ten parameters: crush, crunch, sag, darkness, mix, trim, autogain, attack,
oversampling, bypass.

## Part 2 — resources

Five files in `Resources/`, all exactly 1792 x 592. Rename on the way in —
**JUCE strips hyphens and underscores** from binary data identifiers, so give
them names that survive it:

| Source file | Store as | Becomes | Contents |
| --- | --- | --- | --- |
| `no_clock.png` | `main.png` | `BinaryData::main_png` | base: powered on, lamp lit, bypass lever up, attack LEFT, quality RIGHT, autogain RIGHT, **caps blank** |
| `off.png` | `off.png` | `BinaryData::off_png` | lamp dark + bypass lever down |
| `all_left.png` | `allleft.png` | `BinaryData::allleft_png` | switches in LEFT position |
| `all_center.png` | `allcenter.png` | `BinaryData::allcenter_png` | switches in CENTRE position |
| `all_right.png` | `allright.png` | `BinaryData::allright_png` | switches in RIGHT position |

Delete the previous assets: `panel.png`, `switch-center.png`,
`switch-right.png`, `lamp-on.png`, `toggle-on.png` — from `Resources/` and from
the `SOURCES` list.

### The overlays are sparse — this matters

Each overlay contains **only the switches whose position differs from
`main.png`**. Measured contents:

| Switch | in `allleft` | in `allcenter` | in `allright` |
| --- | --- | --- | --- |
| attack | absent | present | present |
| quality | present | present | absent |
| autogain | present | present | absent |

That is not an error. `main.png` already shows attack LEFT and quality and
autogain RIGHT, so those tiles would be redundant.

**Rule: if the requested position matches the one baked into `main.png`, draw
nothing for that switch; otherwise blit its tile from the matching overlay.**

Baked-in positions: attack LEFT, quality RIGHT, autogain RIGHT.

Do not blindly blit for every position — the source tile does not exist for
three of the eight combinations and you will draw a blank rectangle over the
panel. Equally, do not assume the old rule that "left means draw nothing" — it
is only true for attack now.

Overlap has been verified visually for all three switches in all available
positions: each tile fully covers the baked-in slider, no edge of the
underlying position shows through.

## Part 3 — geometry

All values measured from `main.png` in design space 1792 x 592.

### Knobs

| Parameter | Cap colour | Centre x | Centre y | Cap radius |
| --- | --- | --- | --- | --- |
| `crush` | white | 198 | 406 | 45 |
| `crunch` | black | 434 | 408 | 45 |
| `sag` | black | 667 | 408 | 45 |
| `darkness` | black | 895 | 408 | 45 |
| `mix` | red | 1338 | 408 | 45 |
| `trim` | blue | 1584 | 406 | 45 |

Taken from a marked-up copy of the panel where the user drew a red circle on
each cap face. Centres land within 2 px of each other vertically, so the row is
level. Verified by plotting the pointer at minimum, centre and maximum on every
knob - it stays inside the cap with margin at both extremes.

Do not re-derive these by thresholding dark pixels: the shadow under each cap
falls into the mask and pulls the centroid 20-40 px low. That was tried, then
over-corrected by eye in the other direction, before the marked-up file settled
it.

Rotary range: `setRotaryParameters(pi * 1.25, pi * 2.75, true)` — 270 degrees,
minimum lower left, maximum lower right.

### Switch tiles

Union of the tile across all available positions, 3 px margin already added.

| Control | Sits above | x | y | w | h |
| --- | --- | --- | --- | --- | --- |
| `attack` | sag | 639 | 271 | 78 | 54 |
| `quality` | mix | 1298 | 270 | 63 | 56 |
| `autogain` | trim | 1545 | 270 | 59 | 60 |

`attack` and `quality` are three-position. `autogain` is two-position — use
LEFT for off and RIGHT for on, never centre.

### Lamp and bypass tiles

Both come from `off.png` and are drawn only when bypass is engaged.

| Element | x | y | w | h |
| --- | --- | --- | --- | --- |
| lamp (dark) | 1442 | 115 | 79 | 79 |
| bypass lever (down) | 1572 | 108 | 64 | 101 |

### Text

**Do not draw any text.** Everything is already painted into the artwork:
the "NOT SURE" and "HITROWS" wordmarks, the knob names along the bottom, and
the switch legends "ATTACK 0.3 / 4.0", "QUALITY 1x / 4x", "AUTO GAIN".

Remove the text-drawing code from the previous editor. Keep the tooltips —
`juce::TooltipWindow` plus `setTooltip` on every control, showing name and
current value.

## Part 4 — behaviour

### Editor

Base size 1792 x 592 (same as before). Open at half that: **896 x 296**. Resizable with the
aspect ratio locked to 1792:592 — use `setFixedAspectRatio` on a
`ComponentBoundsConstrainer`. Sensible limits: 700 to 1792 wide.

Compute one `scale = getWidth() / 1792.0f` in `resized()` and derive every
position from it.

### Knobs

The caps in `main.png` are blank — no painted pointer. The live pointer drawn
here is the only one, so it has to read clearly at every angle.

### Pointer

Matched to the marks that were painted on the caps in the earlier artwork, so
it reads as part of the moulding rather than an overlay. Measured from those:

- **Length**: from `0.04 * r` to `0.74 * r`. Shortened from an earlier 0.02 to
  0.88, which ran too close to the rim. It should still read as a confident
  line, not a dot - do not shorten it further.
- **Width**: `0.085 * r`, rounded caps.
- **Colour**: depends on the cap. On the four dark caps and the red and blue
  ones, an off-white `0xffcec7b4` at about 84% alpha. On the white `crush` cap
  the mark is **darker than its background** - a dirty groove rather than a
  highlight - so use `0xff60543a` at about 88%.
  Decide per knob from a fixed table, not by sampling the image at runtime.
- **Shadow**: offset 1 px right and 1.5 px down, `0xff1c1610` at about 43%,
  same width. Reads as an incised groove.
- Keep the line **solid**. Breaking it into segments to fake wear was tried and
  looks like a dotted line at normal size.

All offsets scale with the editor.

`juce::Slider` in `RotaryHorizontalVerticalDrag` mode, fully transparent, sized
to the cap's bounding box. A custom `LookAndFeel_V4` subclass overrides
`drawRotarySlider` to draw **only** the pointer — no body, no ring, no track.

### Lamp

Lit whenever the plugin is not bypassed, dark when bypassed. Straight binary,
no animation.

(If you want the lamp to flicker with gain reduction later, `LimiterCore`
already exposes `getGainReductionDb()` — but do not do it now, it was not
asked for.)

### Bypass lever

Clicking it toggles the bypass parameter. Blit `toggle-on.png`'s tile when
active.

---

## Part 5 — performance

The user explicitly asked for a plugin that is not heavy, and an open editor is
where that is usually thrown away.

- The background is a large image. Draw it once into a cached image sized to
  the current bounds, and rescale only in `resized()` — never rescale a
  1792-wide image on every `paint()`.
- Use `setBufferedToImage(true)` on any static child component.
- No `Timer` unless something actually animates. The lamp changes only when
  bypass changes, so drive it from a parameter listener, not a timer.
- If a repaint is needed for one element, repaint that rectangle, not the
  whole editor.
- `setOpaque(true)` on the editor — the panel covers everything.

---

## Part 6 — acceptance

1. Builds clean, `auval -v aufx Nsur Htrw` passes, 10 parameters.
2. All six knobs turn and drive their parameters; the drawn pointer stays
   inside the coloured cap across the whole range.
3. Each switch shows three distinct positions (two for autogain), and no edge
   of the baked-in position shows through any overlay.
4. Setting a switch to the position already baked into `main.png` draws no
   tile and leaves the panel untouched — no blank rectangle.
5. Lamp goes dark and the lever drops on bypass, both return when released;
   the host's own bypass button does the same thing.
6. No text is drawn by code — the artwork supplies all lettering.
7. Window resizes without distortion; artwork stays sharp at 700 wide.
8. With the editor open and audio playing, CPU is not measurably higher than
   with it closed.
