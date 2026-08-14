# State versioning and version tooltip — Not Sure

Target version: **0.8.4**.

Two unrelated things in one pass. Part 1 matters and is nearly invisible;
Part 2 is small and fun.

---

## Part 1 — stamp a schema version into saved state

### Why now

Parameter values already recall correctly across updates: state is written as
XML keyed by parameter ID, and as long as the IDs hold, values come back.
Removing a parameter (we did it twice) and adding one (`bypass`) are both safe.

The sound is a different matter. Between 0.6 and 0.8 the meaning of those
values changed several times — the drive floor moved from −30 to −42 dB, auto
gain was recalibrated twice, the waveshaper was normalised, the sidechain
filter went from 6 to 24 dB/octave. A project mixed on 0.6 does not sound the
same on 0.8 even with identical knob positions.

For a beta that is fine; we were fixing mistakes. Once the plugin is sold, it
stops being fine — an update that shifts someone's finished mix is damage, not
an improvement.

The thing that makes compatibility fixable later is a version marker in the
saved state. There is none today, so a state written by 0.6 is
indistinguishable from one written by 1.0. Add it now: it costs almost nothing
and cannot be added retroactively.

### What to write

In `getStateInformation`, before serialising, set a property on the root of the
APVTS state:

```
stateSchema = 1
```

Also write the producing plugin version as a separate property, purely for
diagnostics — it makes bug reports readable.

Keep the current XML shape otherwise. Do not restructure it.

### What to read

In `setStateInformation`:

- No `stateSchema` property → the state came from 0.8.3 or earlier. Treat it as
  schema 0 and load it as-is. That is the correct behaviour today; the point is
  that later versions will be able to tell.
- `stateSchema` equal to the current one → load normally.
- `stateSchema` **higher** than this build understands → load what can be
  loaded and ignore the rest. Do not refuse, do not clear the state, do not
  crash. Someone opening a project made on a newer build should get something
  reasonable rather than a reset plugin.

Leave a clear comment at the read site explaining that this is where future
compatibility shims go, so it is obvious where to add one.

### Harden the load path while in there

Current code checks the tag name and nothing else. A state can also arrive
truncated, from a different plugin, or with values outside the current ranges.

- Reject XML that does not parse, or whose root tag does not match — already
  done, keep it.
- Clamp every incoming value into the parameter's current range rather than
  trusting it.
- Never let a malformed state throw out of `setStateInformation`. A host
  loading a corrupted project must get a plugin at defaults, not a crash.

Test with: an empty block, random bytes, a valid XML from a different plugin, a
state with a value far outside a range, and a truncated state.

---

## Part 2 — version on the bottom-right screw

Hovering the bottom-right corner screw shows the plugin version as a tooltip.
A small easter egg — no visible affordance, no cursor change, nothing drawn on
the panel.

Position in design space 1792 x 592:

```
centre (1715, 530), radius 30
```

Round hit area, not a rectangle — the corner of the panel and the chassis edge
are close by, and a square would catch clicks meant for the window edge.

Tooltip text: the version string only, e.g. `0.8.4`. Take it from
`JucePlugin_VersionString` rather than a literal, so it can never drift from
the build.

Nothing happens on click. It is not a button.

---

## Acceptance

1. A project saved by this build and reopened recalls every parameter exactly.
2. A project saved by 0.8.3 (no schema property) still loads, with all values
   intact.
3. A state with `stateSchema = 99` loads without crashing and without wiping
   the user's settings.
4. Empty, random, truncated and foreign state blocks all leave the plugin at
   defaults with no crash.
5. Hovering the bottom-right screw shows the version; hovering a few pixels
   outside it shows nothing.
6. The version shown matches the built version — verify by bumping the version
   and rebuilding, not by reading the source.
