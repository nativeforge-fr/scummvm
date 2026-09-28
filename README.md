# Versailles 1685 — Native Forge Edition · source code

[![Latest release](https://img.shields.io/github/v/release/nativeforge-fr/scummvm?label=release&color=e8791e)](https://github.com/nativeforge-fr/scummvm/releases/latest)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue)](COPYING)

📦 **Looking for the game?** → [**Download the latest release**](https://github.com/nativeforge-fr/scummvm/releases/latest) · 🖥️ [Project page](https://github.com/nativeforge-fr/versailles-1685-native) · 🐛 [Report a bug](https://github.com/nativeforge-fr/versailles-1685-native/issues)

This repository is the **source code** of the Native Forge edition of *Versailles 1685 – Complot à la Cour du Roi Soleil* (Cryo Interactive, 1996): a fork of [ScummVM](https://www.scummvm.org)'s `cryomni3d` engine, built as a standalone native edition for modern systems — no emulation.

- All Native Forge changes live on the **`versailles-fork`** branch (default), one commit per change.
- Upstream base: [scummvm/scummvm](https://github.com/scummvm/scummvm) at commit `1645a096`.
- Upstream README and documentation: https://github.com/scummvm/scummvm#readme

> **Unofficial, non-commercial fan project.** *Versailles 1685* © 1996 Cryo Interactive.
> **No game data or assets are in this repository** — you need your own copy of the game.

*[Version française plus bas.](#français)*

## What this fork adds

- **Standalone build** (`VERSAILLES_STANDALONE`): a dedicated `versailles.exe` with the Native Forge icon (the original game icon is not redistributed).
- **16:9 widescreen**: full-width HUD, adaptive letterbox bars (solid color for logos/title screens, ambient blur for scenes), crisp menus and documentation.
- **In-game language switching** (FR / EN / DE / ZH): separate voice and text languages, subtitles, translated menus (incl. Chinese Big5/CP950).
- **Quality of life**: fullscreen on start, optional bilinear filtering, persistent settings.
- **Gamepad** keymap (click, toolbar, skip).
- **Fixes**: several crashes (toolbar over still images, language switching, message boxes, screen bounds) and display artifacts.

## Why this instead of ScummVM?

ScummVM already runs Versailles — and **neither ScummVM nor this build is an emulator**: both run the game as native code reading your original data. This fork is a *specialized, packaged edition* of that engine: one-click setup, plus 16:9 widescreen and in-game language switching, which stock ScummVM does not offer. Everything the engine itself does is thanks to the ScummVM team; fixes from this fork may be contributed upstream.

## Platforms

- **Windows 10 / 11, 64-bit** — tested and supported.
- **Android ARM64** and **Linux ARM64 (AppImage)** — experimental, untested.

Details and files on the [release page](https://github.com/nativeforge-fr/scummvm/releases/latest).

## Building

Standalone Windows build via MSYS2 / MinGW-w64, with the `cryomni3d` engine and the `VERSAILLES_STANDALONE` flag. See ScummVM's build documentation ([`doc/`](doc), `./configure --help`) for the base toolchain. Game data must come from your own CD / ISO of *Versailles 1685*.

## License

**GPL-3.0** — see [COPYING](COPYING). This repository is the complete corresponding source code of the binaries distributed by Native Forge (GPL v3, section 6).

## Credits

- **ScummVM Team** — `cryomni3d` engine and the ScummVM framework (see [AUTHORS](AUTHORS)).
- **Cryo Interactive** — the original *Versailles 1685* (© 1996).
- **Native Forge** — this edition. ☕ Support: https://ko-fi.com/nativeforge

---

# Français

# Versailles 1685 — Édition Native Forge · code source

📦 **Tu cherches le jeu ?** → [**Télécharger la dernière release**](https://github.com/nativeforge-fr/scummvm/releases/latest) · 🖥️ [Page du projet](https://github.com/nativeforge-fr/versailles-1685-native) · 🐛 [Signaler un bug](https://github.com/nativeforge-fr/versailles-1685-native/issues)

Ce dépôt contient le **code source** de l'édition Native Forge de *Versailles 1685 – Complot à la Cour du Roi Soleil* (Cryo Interactive, 1996) : un fork du moteur `cryomni3d` de [ScummVM](https://www.scummvm.org), compilé en édition native autonome pour les systèmes modernes — sans émulation.

- Toutes les modifications Native Forge sont sur la branche **`versailles-fork`** (par défaut), un commit par changement.
- Base upstream : [scummvm/scummvm](https://github.com/scummvm/scummvm), commit `1645a096`.

> **Projet de fan non officiel et non commercial.** *Versailles 1685* © 1996 Cryo Interactive.
> **Aucune donnée ni asset du jeu dans ce dépôt** — il faut posséder sa propre copie du jeu.

## Ce que ce fork ajoute

- **Build autonome** (`VERSAILLES_STANDALONE`) : un `versailles.exe` dédié avec l'icône Native Forge (l'icône d'origine du jeu n'est pas redistribuée).
- **Écran large 16:9** : HUD pleine largeur, barres letterbox adaptatives, menus et espace documentaire nets.
- **Changement de langue en jeu** (FR / EN / DE / ZH) : voix et textes séparés, sous-titres, menus traduits (dont le chinois Big5/CP950).
- **Confort** : plein écran au démarrage, filtrage bilinéaire optionnel, réglages persistants.
- **Manette** : keymap dédié (clic, toolbar, skip).
- **Corrections** : plusieurs crashs et artefacts d'affichage.

## Pourquoi cette version plutôt que ScummVM ?

ScummVM fait déjà tourner Versailles — et **ni ScummVM ni ce build ne sont de l'émulation** : les deux exécutent le jeu en code natif qui lit tes données d'origine. Ce fork est une *édition spécialisée et packagée* de ce moteur : installation en un clic, plus l'écran large 16:9 et le changement de langue en jeu, absents de ScummVM standard. Le moteur lui-même est le travail de l'équipe ScummVM ; les correctifs de ce fork pourront être proposés en amont.

## Compiler

Build Windows autonome via MSYS2 / MinGW-w64, avec le moteur `cryomni3d` et le drapeau `VERSAILLES_STANDALONE`. Voir la documentation de build de ScummVM ([`doc/`](doc), `./configure --help`). Les données du jeu doivent provenir de ton propre CD / ISO.

## Licence

**GPL-3.0** — voir [COPYING](COPYING). Ce dépôt est le code source correspondant complet des binaires distribués par Native Forge (GPL v3, section 6).

## Crédits

**ScummVM Team** (moteur, voir [AUTHORS](AUTHORS)) · **Cryo Interactive** (jeu d'origine, © 1996) · **Native Forge** (cette édition). ☕ Soutenir : https://ko-fi.com/nativeforge
