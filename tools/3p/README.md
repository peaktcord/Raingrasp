# Vendored third-party sources

Single-file libraries and tools checked in verbatim. Record provenance here when
adding one.

| File | Version | Source | SHA-256 |
|---|---|---|---|
| `stb_image_write.h` | v1.16 | [nothings/stb](https://github.com/nothings/stb) | (pre-existing) |
| `stb_image.h` | v2.30 | [nothings/stb @ f58f558](https://github.com/nothings/stb/blob/f58f558c120e9b32c217290b80bad1a0729fbb2c/stb_image.h) | `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3` |

`stb_image.h` decodes the game's PNG art for the SDL3 backend; SDL3 core has no
image loader. Both stb headers offer a public-domain/MIT choice; Raingrasp uses
the MIT option recorded in `STB-LICENSE.txt`.
