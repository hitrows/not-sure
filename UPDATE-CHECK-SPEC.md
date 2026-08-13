# Update check specification — Not Sure

Target version: **0.8.1**.

The plugin checks whether a newer version exists and says so. It does **not**
download, install, or update anything.

That limit is deliberate, not laziness:

- The bundle is memory-mapped while the host has it loaded. Overwriting it on
  disk crashes the host, now or at the next call into it. A plugin cannot
  unload itself to get around this.
- The plugins install to `/Library/Audio/Plug-Ins`, which needs an admin
  password. Anything that asks for one mid-session looks exactly like malware.
- Logic sandboxes plugins through AUHostingService. Network and file access are
  restricted, and the restrictions differ between hosts.

Every serious vendor splits this the same way — the plugin reports, a separate
app installs (Native Access, iZotope Product Portal, Plugin Alliance
Installation Manager). Do not add self-installing behaviour later without
raising it first.

---

## Part 1 — the manifest

A small JSON file on a static host. GitHub Pages or a raw gist is fine; no
server needed.

```json
{
  "version": "0.9.0",
  "url": "https://github.com/<user>/not-sure/releases/latest",
  "notes": "Fixed preset recall in Cubase"
}
```

Host it in the project's own GitHub repo. `raw.githubusercontent.com` serves it
directly with no Pages setup needed — one less thing to configure, and it works
the moment the file is pushed. The `url` points at the Releases page, which is
where the `.pkg` lives.

`notes` is optional and currently unused by the UI — it is there so a future
version can show it without changing the format.

Put the URL in one named constant. Do not scatter it.

---

## Part 2 — when and how often

**One check per application launch, shared across every instance.** A session
can easily hold twenty instances of the plugin; twenty requests on every editor
open is telemetry, not a version check.

Implement as a single shared object — a `SharedResourcePointer` or an explicit
singleton owned by the processor — not per-editor state.

Cache the result in a `juce::PropertiesFile` under

```
~/Library/Application Support/Hitrows/Not Sure/settings.xml
```

storing the last check time and the last seen version. **Do not check again if
the cached check is under 24 hours old** — read the cache and use it.

The cached version is what the notice is drawn from, so once a check has found
an update the notice keeps appearing offline and after restarts, until the
installed version catches up.

Trigger the check when the first editor opens, never in the constructor of the
processor. A host scanning plugins at startup instantiates every plugin it
knows about; scanning must not cause network traffic.

---

## Part 3 — threading

This is where it goes wrong. Requirements:

- The fetch runs on a background thread. `juce::ThreadPoolJob` or a
  `juce::Thread`, never the message thread and obviously never the audio
  thread.
- The editor opens immediately and does not wait for a result. If the answer
  arrives later, the notice fades in; if it never arrives, nothing happens.
- Timeout of about 5 seconds. Then give up silently.
- **The editor can close while the request is in flight.** The completion must
  not touch a dangling editor — deliver the result to the shared object, and
  have editors query it, rather than capturing an editor pointer in the job.
- On any failure — offline, DNS failure, 404, malformed JSON, non-numeric
  version — do nothing at all. No dialog, no console spam, no retry loop. A
  tester on a plane must see exactly the normal plugin.

---

## Part 4 — comparing versions

Parse `major.minor.patch` into three integers and compare numerically.

Do not compare version strings lexically: `"0.10.0" < "0.9.0"` is true as
strings and false as versions, and we will reach 0.10 within a few releases.

If the manifest version does not parse, treat it as no update.

---

## Part 5 — the notice

Not drawn text — an artwork overlay, `Resources/newver.png`, already in the
project. Same 1792 x 592 canvas as the panel, so it composites at the same
scale with no offset maths. Add it to the `juce_add_binary_data` SOURCES list;
it arrives as `BinaryData::newver_png`.

Blit it over the panel **only when an update is available**. When there is
none, draw nothing — the panel looks exactly as it does today.

The red lettering sits at **x 737..1052, y 99..118** in design space.

### Click target

The user asked for the text plus 5 px in every direction:

```
x 732, y 94, w 326, h 30
```

Clicking anywhere in that rectangle opens the download URL in the default
browser. Change the mouse cursor to a pointing hand over it so it reads as
clickable.

One practical caveat to raise with the user rather than silently change: at the
default editor width of 896 that rectangle is only about 14 physical pixels
tall, and at the 700 minimum it is 11. That is clickable but tight. Widening
the vertical margin to about 12 design px (`y 87, h 44`) would make it
comfortable while staying inside the scratched area of the overlay, so nothing
looks off. Ask before changing it.

The overlay is not part of the cached background: it may arrive after
`resized()` has already run. When the result comes in, repaint only its
rectangle rather than rebuilding the whole background image.

### It does not go away

The notice stays visible for as long as the plugin runs and across every
session, until the installed version actually matches the latest one. There is
no dismiss, no close button, no right-click to hide it. It occupies dead metal
in the artwork and blocks nothing.

This falls out of the caching rule below rather than needing extra state: the
cache holds the latest known version, and the notice is drawn whenever that is
newer than the running build. So it also shows correctly with no network at
all, once a check has succeeded even once.

## Part 6 — privacy

Even a version check sends the user's IP address and the fact that they opened
the plugin. That is telemetry, however small, and testers should not discover
it by reading a packet dump.

- Add a short section to `README-BETA.md` in Russian saying what is sent (an
  HTTP request for a static file, nothing about the user, no analytics), how
  often (once a day at most), and how to turn it off.
- Send no identifying information: no serial, no machine ID, no usage data, no
  query parameters. A plain GET of a static file.
- The notice itself is deliberately not dismissible, but the **network check**
  must still be switchable off — these are different things. A key in the same
  properties file (`checkForUpdates`, default true) disables the request
  entirely. It has no UI for now; document it in the README so anyone who
  objects has a way out.

---

## Acceptance

1. With the network unplugged, the plugin opens normally and shows no notice,
   no error, and no delay.
2. With a manifest whose version is lower than or equal to the local one, no
   notice appears.
3. With a higher manifest version, the notice appears and opens the URL when
   clicked.
4. Opening twenty instances in one session produces **one** request. Verify
   with a proxy or by logging, not by assumption.
5. A second launch within 24 hours produces no request.
6. Closing the editor while a request is in flight does not crash. Test by
   pointing the URL at a deliberately slow endpoint and closing immediately.
7. Once an update is known, the notice still appears with the network
   unplugged and after a restart.
8. A plugin scan by the host produces no network traffic.
9. `0.10.0` is correctly treated as newer than `0.9.0`.
