# Release Notes

## Current Release

Artifact:

```text
release/kindle-iagno-extension.zip
```

Verify:

```bash
cd release
sha256sum -c SHA256SUMS
```

Contents:

- ARM hard-float `kindle-iagno` executable.
- KUAL extension metadata and launch scripts.
- Iagno visual assets copied from GNOME Games.
- Bundled GTK2/Cairo runtime library set copied from the ARM Docker builder.
- License and third-party runtime notices.

Known constraints:

- This is an unofficial derivative/adaptation release, not an official GNOME or
  GnomeGames4Kindle release.
- Requires a jailbroken Kindle with KUAL.
- Kindle home-screen `.sh` tapping is not reliable unless another launcher/file
  association is installed. Use KUAL.
