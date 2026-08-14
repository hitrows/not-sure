# Licensing specification — Not Sure

Target version: **0.9.0**.

---

## The model

GOG-style. A purchase gets you an installer and a small licence file. Install
on as many machines as you like, as many times as you like, forever, with no
internet. No activation server, no machine binding, no seat count, no phoning
home.

**The plugin is fully functional without a licence file.** No feature gating,
no silence bursts, no nag dialogs, no time limit. Everything works.

The only difference a licence makes is a line of text:

- no valid licence → `unregistered`
- valid licence → `licensed to <name>`

### Be clear about what this is

This is **not** copy protection, and it should not be described internally as
if it were. Because the plugin works fully without a file, anyone sharing a
copy simply omits the file — the watermark deters nothing.

What it actually is: a small thing a buyer gets that a non-buyer does not. That
is a deliberate choice, made with the trade-off understood. Do not later
"strengthen" it into enforcement without asking — that would change the product
the early buyers paid for.

Consequence: **there is no demo build.** The shipped plugin is the demo. One
installer for everybody.

---

## The licence file

Location — the same folder the update checker already uses:

```
~/Library/Application Support/Hitrows/Not Sure/licence.txt
```

Plain text, three fields: name, email, signature.

The signature exists so the name cannot simply be typed in by hand. Without it
the watermark carries no meaning at all, since anyone could write any name.

Use `juce::RSAKey` — built into JUCE, no new dependency, and the security bar
here is deliberately low. Sign the concatenation of name and email; embed the
**public** key in the plugin.

### The private key must never reach the repo

**The repository is public.** Committing the signing key would let anyone mint
licences in anyone's name, which is the one way this scheme can actually fail.

- Generate the key pair once, store the private half outside the project
- Add the key filename to `.gitignore` regardless, as a second line of defence
- Back it up somewhere durable — losing it means every future licence has to be
  reissued against a new key, and old files stop verifying

---

## Reading it

- Read once, when the processor is constructed or the first editor opens.
  Never on the audio thread, never per block.
- Result is a string, held in the same shared object the update checker uses.
- Any failure — file absent, unreadable, malformed, signature invalid, wrong
  key — yields `unregistered`. Silently. No dialog, no log spam, no difference
  in behaviour.
- Never let a bad file throw out of the constructor.

Test with: no file, empty file, random bytes, valid fields with a corrupted
signature, and a file signed with a different key.

---

## Showing it

On the bottom-right screw tooltip, which already shows the version as of 0.8.4.
Add a second line:

```
0.9.0
licensed to Ivan Petrov
```

or

```
0.9.0
unregistered
```

Same hit area as the version tooltip — centre (1715, 530), radius 30 in the
1792 x 592 design space. Nothing is drawn on the panel itself and nothing
changes visually; the artwork stays as it is.

If the user later wants it engraved on the panel instead, that is a separate
decision — it would mean drawing text over the artwork, which this project
deliberately stopped doing.

---

## Generating licences

A small command-line tool, `tools/make-licence`:

```sh
make-licence "Ivan Petrov" ivan@example.com > licence.txt
```

Keep it a plain C++ tool that links nothing but the JUCE modules it needs, in
the same spirit as `notsure-render`. It reads the private key from a path given
by an environment variable or an argument — never a hard-coded path inside the
repo.

Issuing by hand is correct at this stage. At five sales a week this is one
email; automate it when it becomes tiresome, not before.

---

## Acceptance

1. With no licence file the plugin behaves identically to today in every
   respect, and the tooltip reads `unregistered`.
2. With a valid file the tooltip shows the buyer's name.
3. A file with a corrupted signature reads as `unregistered`, not as valid and
   not as an error.
4. A file signed with a different key reads as `unregistered`.
5. Empty, missing and garbage files all leave the plugin working normally.
6. `git ls-files` shows no private key anywhere in the repository.
7. Nothing in the licence path runs on the audio thread — verify by inspection
   of `processBlock` and everything it calls.
