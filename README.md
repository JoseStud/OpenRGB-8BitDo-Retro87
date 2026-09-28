# OpenRGB-8BitDo-Retro87

**Unofficial OpenRGB fork** that adds support for the **8BitDo Retro 87
Mechanical Keyboard (Mecha BREAK)**, including per-key streaming over its USB
cable. It follows upstream OpenRGB automatically and publishes a Linux
AppImage whenever upstream or the patches change.

This is not the OpenRGB project and is not endorsed by it. Upstream:
<https://gitlab.com/CalcProgrammer1/OpenRGB>.

## How this fork differs from OpenRGB

Everything is in [patches/](patches/), applied on top of upstream `master`:

- **8BitDo Retro 87 device**, over the 2.4 GHz dongle (`2dc8:202e`) or the USB
  cable (`2dc8:2028`): the keyboard's stored effects (Static, Breathing,
  Spectrum Cycle, Rainbow Wave, Ripple, Resonance, Starlight, Off) and the
  stored per-key picture (Custom).
- **Direct mode over the USB cable** through the keyboard's HID LampArray
  interface (Windows Dynamic Lighting): runtime per-key colours that are never
  saved. Leaving Direct hands the lighting back to the keyboard.
- **Flash-wear protection.** Every per-key save on this keyboard erases flash.
  Saved writes are coalesced to at most one per second, and identical per-key
  writes are skipped, so slider drags or fast clients cannot wear it out.
- **HID LampArray on Linux without an interrupt-IN endpoint.** Such
  interfaces get no hidraw node from `usbhid`, so upstream cannot see them;
  a libusb transport and detector handle them. Includes a fix for an
  uninitialised report-ID table and corrections for the Retro 87's wrong
  lamp positions and key bindings.

Full documentation, the protocol research and the Plasma tray integration that
drives this build: <https://github.com/JoseStud/8bitdo-retro87-tools>
(see `OPENRGB.md`).

## Download and use

Take `OpenRGB-8BitDo-Retro87-x86_64.AppImage` from the
[latest release](../../releases/latest), check it against its `.sha256`, and:

```sh
chmod +x OpenRGB-8BitDo-Retro87-x86_64.AppImage
./OpenRGB-8BitDo-Retro87-x86_64.AppImage
```

Linux needs access to the keyboard: install OpenRGB's udev rules
(`./OpenRGB-8BitDo-Retro87-x86_64.AppImage --generate-udev-rules 60-openrgb.rules`,
then copy the file to `/etc/udev/rules.d/` and replug), or use
`install-kde.sh` from the tools repository, which also sets this build up as
a background server for the tray:

```sh
OPENRGB=/path/to/OpenRGB-8BitDo-Retro87-x86_64.AppImage ./install-kde.sh
```

Direct mode and the merged device use libusb and are Linux-only for now.

## How the automatic updates work

[`.github/workflows/release.yml`](.github/workflows/release.yml) runs daily,
on pushes that change `patches/`, and on demand (*Actions → Sync upstream and
release → Run workflow*):

1. Clone upstream OpenRGB `master` from GitLab.
2. Skip everything if a release for this upstream commit and patch set
   (tag `u<upstream>-p<patches>`) already exists.
3. Apply the patches and update the branches
   [`upstream`](../../tree/upstream) (upstream as fetched) and
   [`retro87`](../../tree/retro87) (upstream plus one commit with the patches).
4. Build the AppImage with upstream's own `scripts/build-appimage.sh` in
   upstream's CI container (`openrgb-linux-ci-deb-builder:trixie-amd64`, Qt 6).
5. Publish a release with the AppImage and its SHA-256.

If the patches stop applying or the build fails, nothing is released and an
issue labelled `auto-update` is opened (or updated); fix `patches/`, push, and
the next successful run closes it. If upstream changes its own GitHub
workflow files, the workflow token cannot update the source branches; releases
continue and the branches catch up after a manual push.

## Why `main` holds only patches

`main` has no OpenRGB history: it holds the patches, the workflow and this
README, and the workflow fetches upstream at build time (the patch-queue
layout used by projects such as VSCodium or LibreWolf). The `upstream` and
`retro87` branches are generated for browsing. Because they share no history
with `main`, GitHub shows `retro87` as thousands of commits "ahead" and one
"behind" `main`; the meaningful comparison is
[`upstream...retro87`](../../compare/upstream...retro87), which is always the
single patch commit.

This is deliberate. The token GitHub gives each workflow run
(`GITHUB_TOKEN`) may not push commits that add or change files under
`.github/workflows/`. If the workflow lived on the patched branch itself, the
alternatives would be:

- **Rebase that branch onto upstream.** This rewrites the commit containing the
  workflow file, so the push would need a personal access token (or GitHub App
  token) with workflow permission, created by the owner and stored as a
  repository secret.
- **Merge upstream into it.** This works with the built-in token, but it would
  still be refused whenever upstream changes its own workflow files, and the
  history fills with merge commits.

With the workflow on a separate `main`, updates never rewrite the branch the
workflow lives on and no extra token is needed. When upstream changes its own
workflow files, only the generated `upstream` and `retro87` branches are
refused and lag until pushed by hand; releases continue.

## Updating the patches

This fork stays a patch queue on purpose: the OpenRGB side is expected to
change rarely, and new lighting modes or states are written as SDK clients
(Direct, Custom and the effects already exist), which need no patch change
at all. If the OpenRGB code starts to see active development, a Git-native
fork (the changes as real commits on `retro87`, with upstream merged in)
would suit better.

The patch has a second, identical copy in the tools repository,
[`openrgb/retro87-openrgb.patch`](https://github.com/JoseStud/8bitdo-retro87-tools/blob/main/openrgb/retro87-openrgb.patch),
from which the tools' installer builds OpenRGB locally. The two are kept in
sync by hand:

1. Change and test the code in an OpenRGB checkout on upstream `master`.
2. Export the difference (including new files) and commit it to the tools
   repository, as described in its `OPENRGB.md` ("Maintaining the fork").
3. Copy the same file to `patches/0001-8bitdo-retro87.patch` here and push to
   `main`; the push starts the workflow, which releases a new AppImage.

The release tag ends in the first 10 hex digits of the patch's SHA-256
(`...-p<hash>`), so both copies can be checked with `sha256sum`.

## About the code

The changes were written with an AI assistant (Claude) and tested on one
keyboard (Linux, USB cable). OpenRGB's contribution guidelines do not accept
AI-generated submissions, so this fork is **not intended for upstream
merging**. Use it at your own risk.

## License

GPL-2.0-only, as OpenRGB. See [LICENSE](LICENSE).
