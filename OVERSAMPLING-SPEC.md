# Oversampling policy — Not Sure

Target version: **0.8.3**.

Three changes: cap oversampling at high sample rates, go to maximum quality
during offline render, and stop reporting a moving latency.

---

## Part 1 — fixed latency (do this first)

This is the enabling change and the one with a trap in it.

Right now latency depends on the oversampling factor: 0 samples at 1x, 24 at
2x, 36 at 4x. That is already awkward — switching Quality mid-session makes the
host re-align the track. It becomes an actual bug once the factor can change by
itself, because during an offline bounce the host has already committed to the
latency it was told in realtime. Changing it mid-render shifts the bounced
audio against everything else in the project.

**Always report the maximum latency, 36 samples, whatever factor is running.**

At factors below 4x the oversampler contributes less than that, so the
difference has to be made up internally: pad the wet path (and the existing dry
delay line) so the total is always 36 base-rate samples. The dry delay already
exists in `LimiterCore`; extend it rather than adding a second mechanism.

36 samples is 0.8 ms at 44.1 kHz. For a mixing plugin that is nothing, and it
buys a latency figure that never moves — no host re-alignment, no risk of a
bounce landing off by a few samples.

Report it once in `prepareToPlay` and never call `setLatencySamples` again.
That also closes the known issue where latency was reported as zero in
`prepareToPlay` because the oversampler had not yet been given its factor.

Verify: bounce a project with the plugin at each Quality setting and confirm
the rendered audio lines up sample-accurately with a bypassed reference.

---

## Part 2 — cap by sample rate

Oversampling exists to keep the waveshaper's harmonics from folding back below
Nyquist. At a high enough sample rate there is little left to fold, so running
4x is spending CPU for nothing.

Measured on the offline renderer, same 2 seconds of audio at Quality 4x:

| Sample rate | Time |
| --- | --- |
| 44.1 kHz | 52 ms |
| 96 kHz | 110 ms |
| 192 kHz | 224 ms |

Cap the effective factor:

| Sample rate | Maximum effective factor |
| --- | --- |
| below 64 kHz | 4x |
| 64 to 128 kHz | 2x |
| above 128 kHz | 1x |

The cap is a ceiling, not an override — a user asking for 1x at 44.1 kHz still
gets 1x, because that is the deliberately dirty mode and it is part of the
character. Only downward.

The Quality control keeps showing what the user chose. Put the actual running
factor in its tooltip so the behaviour is discoverable rather than hidden.

---

## Part 3 — maximum quality offline

`AudioProcessor::isNonRealtime()` tells us the host is rendering rather than
playing. During a bounce nobody is counting CPU, so run the highest factor the
sample-rate cap allows, regardless of the Quality setting.

- Read it per block, not once — hosts flip it when a bounce starts and ends.
- Changing factor means resetting the oversampler's filter state, which clicks.
  Do the switch only when the flag actually changes, not every block.
- The sample-rate cap still applies. At 192 kHz offline, 1x remains 1x — there
  is genuinely nothing to gain.
- Latency does not change, because of Part 1. That is the whole point of doing
  Part 1 first.

One exception worth respecting: if the user chose **1x deliberately**, that is
an aesthetic choice — the aliasing is the sound. Overriding it during bounce
would mean the mix does not match what was heard. So offline maximum applies
only when the user asked for 2x or 4x. Leave 1x alone.

---

## Part 4 — verify the universal binary

Never actually checked. Run:

```sh
lipo -archs "build/.../Not Sure.component/Contents/MacOS/Not Sure"
lipo -archs "build/.../Not Sure.vst3/Contents/MacOS/Not Sure"
```

Both should print `x86_64 arm64`. If either shows only one architecture,
`CMAKE_OSX_ARCHITECTURES` is not reaching that target — fix it and say so,
because half the testers may be on Intel.

---

## Part 5 — README

Replace whatever the formats section currently says with the truth:

```
Форматы: AU и VST3, 64-bit
Процессоры: Apple Silicon и Intel (universal)
Частоты дискретизации: 44.1 – 192 кГц
macOS 10.15 и новее
```

No VST2 — Steinberg closed the SDK in 2018 and terminated existing agreements
in 2024; it cannot be licensed at any price.
No AAX — needs an Avid developer account, a Pro Tools Developer build and PACE
signing, which is months of correspondence for a beta.

Do not list formats we do not ship.

---

## Acceptance

1. Reported latency is 36 samples at every Quality setting and never changes
   during a session.
2. A bounce lines up sample-accurately against a bypassed reference, at 1x, 2x
   and 4x.
3. At 192 kHz the plugin runs 1x however Quality is set, and CPU is comparable
   to 44.1 kHz per second of audio rather than four times higher.
4. At 96 kHz the ceiling is 2x.
5. During an offline bounce with Quality at 2x, the plugin runs 4x — verify by
   logging, not by assumption.
6. With Quality at 1x, an offline bounce still runs 1x.
7. `lipo -archs` reports both architectures for both formats.
8. No clicks when a bounce starts or ends.
