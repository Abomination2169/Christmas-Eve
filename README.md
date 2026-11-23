# Christmas Eve v1.0.0 🎄 — Beat Saber Quest 1.40.8

![Christmas Eve preview](ChristmasEve-README.png)

Celebrate Christmas Eve on a peaceful, snowy alpine winter night.

**Status:** Public source and final selected artwork; **no public QMOD or redistributed audio** is included. The four Ogg files used by private Build #131 have been withheld while their redistribution status is reviewed. Gameplay behavior was previously reported working on Quest for private Build #129; this reorganized source snapshot has not been tested on Quest.

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

For code and dependency credits, see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). For complete audio credits and redistribution status, see [MUSIC_LICENSES.md](MUSIC_LICENSES.md). These credits are required acknowledgments where applicable; **credits alone do not resolve permissions for technically extractable bundled files**.

## Source layout and builds

- `src/`, `include/` — C++ implementation and headers.
- `assets/` — current selected PNG artwork and settings used by the build.
- `cmake/`, `scripts/`, `qpm.json`, `qpm.shared.json`, `CMakeLists.txt`, `assets.cmake`, `mod.template.json` — build and packaging files.
- `FileCopies/README.md` — lists the four **omitted** soundtrack/effect files required for a complete QMOD build.
- `upstream/README.md` — retained original-project credits.

A private manual build, **#131**, succeeded at the v1.0.0 release-candidate commit `b15acb86473c518a73f12413a8f68e30344fd9f8`. This public cleanup reorganizes files, restores required source/selected artwork, removes unused variants, and omits the bundled Ogg files. **It is not an exact source/archive match for the private Build #131 binary**, and a matching public QMOD must be freshly built and checked after permissions are resolved. Do not represent this public source snapshot as a separately Quest-tested release.

## Licensing and distribution

The project's covered source code is distributed under [GPLv3](LICENSE). Audio and other third-party content retain their separate licensing terms. No public QMOD release is attached here pending verification of the outstanding audio redistribution rights; see [MUSIC_LICENSES.md](MUSIC_LICENSES.md).
