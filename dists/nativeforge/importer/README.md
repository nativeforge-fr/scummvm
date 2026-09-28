# vfimport — portable Versailles 1685 data importer (Native Forge)

Dependency-free C++17 reimplementation of the **core** of the Windows PowerShell
installer's extraction pipeline, so the *same* logic runs on **Windows, Linux
(ArmadaOS / handhelds) and Android**. It is the shared foundation the per-platform
installers (Linux GUI, Android app) build on.

**No game data is shipped.** vfimport reads the user's own CD/ISO of *Versailles
1685* (Cryo, 1996) and produces a `game_data` folder the standalone engine boots.

## What it does
- **ISO9660 reader** with **MODE2/2352 raw de-raw** — no `7z`, no `.NET`.
- **Edition fingerprint**: MD5 of the 14 signature files, first 8 hex chars each
  (112 chars total). Byte-for-byte identical to the PowerShell/`Get-FileHash`
  pipeline, so `known_editions.json` fingerprints are reusable as-is.
- **Disc type detection**: `edition` (CD 1), `data` (CD 2), `multilang` (DVD).
- **Base extraction**: everything under `DATAS_V\` and `INSTALL\` (with
  `INSTALL\DATAS_V\` → `INSTALL\DATA\`) merged from one or more ISOs into
  `game_data`.
- **Multi-language build** (`build`): a base language at the root plus any number
  of overlay languages under `lang/<code>/`, each **diffed against the base**
  (only differing files kept), with **shared-voice dedup** (e.g. Chinese/Korean
  reuse the English voice track, extracted once) and **CJK font** deployment
  (bundled Noto Sans CJK + rewritten `.LST`). Overlay sources may be CD editions
  or a multilang **DVD** (`--prefix <FOLDER>`).

Parity with the Windows installer's extraction pipeline, validated against real
ISOs (fingerprints byte-identical; FR+EN+ZH and DVD Italian builds correct).

## Build
```
make                 # -> vfimport
# or:  c++ -std=c++17 -O2 -o vfimport vfimport.cpp
```

## Usage
```
vfimport fingerprint <iso> [dvd-prefix]     # 112-char edition fingerprint
vfimport type        <iso>                  # edition | data | multilang <langs...>
vfimport list        <iso> [prefix]         # list internal files
vfimport extract     <game_data_dir> <iso> [<iso2> ...]
```

Example (French edition, CD 1 + CD 2):
```
vfimport extract ./game_data VERSAILL_1.iso VERSAILL_2.iso
```
