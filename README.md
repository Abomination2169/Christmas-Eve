# Christmas Eve v1.0.0 — Beat Saber Quest 1.40.8

Celebrate Christmas Eve on a peaceful, snowy alpine winter night.

**Status:** Private release candidate. Not approved for public redistribution until bundled audio rights are confirmed. The gameplay and presentation are based on Quest-tested private Build #129; the new v1.0.0 metadata package has not yet been tested on Quest.

## Features

- Festive red-and-green BEAT SABER neon logo, including the preserved custom E artwork.
- Evergreen garland and warm-gold sparkles, candy cane, decorated tree and presents.
- Snowy alpine panorama, foreground snowdrifts and layered falling snow.
- Three rotating Christmas menu music tracks.
- Three gently animated PNG-backed aurora bands on the results screen **only** for S, SS or SSS rank, with a one-shot bell jingle.
- S/SS/SSS aurora results suppress firework particles, explosion sounds and firework-associated light flashes. Lower ranks retain their existing behavior.
- Original Christmas Eve environment lighting (no experimental midnight/emerald menu recoloring).

Compatible with **Beat Saber Quest 1.40.8_7379**. Internal mod ID: `christmas`; QMOD version: **1.0.0**.

## Authors and attribution

Christmas Eve builds upon the original Christmas Theme by **CatsaCode and laynia**, preserving the original snowdrift contributions of **Furzz and Laynia**. Original project documentation is retained in [upstream/README.md](upstream/README.md). Source licensing remains **GPL-3.0**.

For complete audio credits and redistribution status, see [MUSIC_LICENSES.md](MUSIC_LICENSES.md). These credits are required acknowledgments where applicable; **credits alone do not resolve permissions for technically extractable bundled files**.

## Testing and distribution

Private Build #129 at commit `2549eded97fe9493e73ea4bc9fd4baec2aed4e52` was reported by the project owner to work on Quest. The v1.0.0 release candidate changes only metadata and documentation, but it still requires a fresh private build/package check. Public publishing remains blocked on outstanding audio-rights evidence.

Run only the private manual `test-qmod.yml` on `release/christmas-eve-v1.0.0-candidate`. Validate the `mod.json` version and package target, ARM64 library, PNG artwork and all four bundled OGG files. No public release is authorized solely by a successful build.
