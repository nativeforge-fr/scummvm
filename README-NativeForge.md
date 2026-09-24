# Versailles 1685 — native Windows edition (ScummVM cryomni3d fork) · Native Forge

*[Version française plus bas.](#versailles-1685--édition-native-windows-fork-scummvm-cryomni3d--native-forge)*

This repository is a **fork of the `cryomni3d` engine from
[ScummVM](https://www.scummvm.org)**, modified by **Native Forge** to produce a
**standalone native edition for modern Windows** of the game *Versailles 1685 –
Complot à la Cour du Roi Soleil* (Cryo Interactive, 1996), **without emulation**.

The original game was a 16-bit executable that no longer runs on today's 64-bit
Windows. This fork builds a standalone `versailles.exe` binary (based on ScummVM's
cryomni3d engine) that reads the game data and runs it natively.

> **Unofficial fan project — non-commercial.**
> *Versailles 1685* and its data are © 1996 Cryo Interactive and its rights holders.
> This repository contains **no game data or assets** whatsoever: you must own your
> own disc / disc image of the game.

## License

Like ScummVM, this project is licensed under the **GNU General Public License v3**
(see [`COPYING`](COPYING)). This repository is the **corresponding source code** for
the `versailles.exe` binary distributed by Native Forge, as required by GPL v3.

- Upstream: https://github.com/scummvm/scummvm
- Upstream base of this fork: commit `1645a096`
- All Native Forge changes live on the **`versailles-fork`** branch.

## What this fork adds (over stock cryomni3d)

Around 1,200 lines across ~24 files:

- **16:9 / widescreen display**: full-width HUD, adaptive letterbox bars (solid color
  for logos/title screens, ambient blur for scenes), crisp menus and documentation.
- **In-game multilingual**: hot language switching (FR / EN / DE / ZH), separate voice
  vs. text language, subtitles, translated menu labels (incl. Chinese Big5/CP950).
- **Quality of life (standalone)**: fullscreen on start, optional bilinear filtering
  (smooth upscale), settings persisted to the global domain.
- **Gamepad**: in-game keymap (click, toolbar, skip).
- **Fixes**: several crashes (toolbar over still images, language switching, message
  boxes, screen bounds) and display artifacts.
- **Icon**: the standalone executable uses the Native Forge icon
  (`icons/nativeforge.ico`). The original game icon is **not** redistributed.

Full detail is in the `versailles-fork` branch history (one commit per change).

## Why this instead of ScummVM?

ScummVM already runs Versailles through its cryomni3d engine — and, to be clear,
**neither ScummVM nor this build is an emulator**: both run the game as native code
that reads your original data. This project is a *specialized, packaged edition* of
that engine, for people who just want to play this one game with no setup:

- **One-click, zero-config**: a single installer reads your CD/ISO and sets everything
  up as a standalone "Versailles" app — no ScummVM install, no adding games, no
  locating data files.
- **16:9 widescreen** with adaptive letterbox bars — *not available in stock ScummVM*.
- **In-game language switching** (FR/EN/DE/ZH) with separate voice & text — *not
  available in stock ScummVM*.
- **Defaults tuned for this game**: fullscreen, smooth upscaling, a game-specific
  gamepad keymap, persistent settings.

Everything the engine itself already does is thanks to the ScummVM team. If you want a
cross-platform, multi-game, actively maintained solution, use ScummVM. If you want a
ready-to-play native Windows edition of *Versailles 1685* with widescreen and in-game
language switching, use this. Fixes from this fork may be contributed upstream.

## Building

Standalone Windows build via MSYS2 / MinGW-w64, with the cryomni3d engine and the
`VERSAILLES_STANDALONE` flag. See ScummVM's build docs ([`doc/`](doc),
`./configure --help`) for the base toolchain. This repository does not provide the
game data — it must come from your own copy (CD / ISO) of *Versailles 1685*.

## Credits

- **ScummVM Team** — cryomni3d engine and ScummVM framework (see [`AUTHORS`](AUTHORS)).
- **Cryo Interactive** — original *Versailles 1685* game (© 1996).
- **Native Forge** — this fork, the native edition and packaging.

Support Native Forge: https://ko-fi.com/nativeforge

---

# Versailles 1685 — édition native Windows (fork ScummVM cryomni3d) · Native Forge

Ce dépôt est un **fork du moteur `cryomni3d` de [ScummVM](https://www.scummvm.org)**,
modifié par **Native Forge** pour produire une **édition native autonome pour Windows
moderne** du jeu *Versailles 1685 – Complot à la Cour du Roi Soleil* (Cryo Interactive,
1996), **sans émulation**.

Le jeu d'origine était un exécutable 16 bits qui ne fonctionne plus sur les Windows
64 bits actuels. Ce fork compile un binaire `versailles.exe` autonome (basé sur le
moteur cryomni3d de ScummVM) qui lit les données du jeu et le fait tourner nativement.

> **Projet non officiel réalisé par un fan — projet non commercial.**
> *Versailles 1685* et ses données sont © 1996 Cryo Interactive et leurs ayants droit.
> Ce dépôt ne contient **aucune donnée ni aucun asset du jeu** : il faut posséder son
> propre disque / sa propre image du jeu.

## Licence

Comme ScummVM, ce projet est sous **GNU General Public License v3** (voir
[`COPYING`](COPYING)). Ce dépôt constitue le **code source correspondant** au binaire
`versailles.exe` distribué par Native Forge, conformément à la GPL v3.

- Upstream : https://github.com/scummvm/scummvm
- Base upstream de ce fork : commit `1645a096`
- Toutes les modifications Native Forge sont sur la branche **`versailles-fork`**.

## Ce que ce fork ajoute (par rapport au cryomni3d d'origine)

Environ 1200 lignes sur ~24 fichiers :

- **Affichage 16:9 / widescreen** : HUD pleine largeur, barres latérales adaptatives
  (couleur unie pour les logos/écrans-titres, flou ambiant pour les scènes), rendu
  net des menus et de l'espace documentaire.
- **Multilingue en jeu** : bascule de langue à chaud (FR / EN / DE / ZH), séparation
  langue des voix / langue des textes, sous-titres, libellés de menu traduits (dont
  le chinois Big5/CP950).
- **Confort de jeu (standalone)** : démarrage en plein écran, filtrage bilinéaire
  optionnel (upscale lissé), persistance des réglages dans le domaine global.
- **Manette** : keymap de jeu (clic, toolbar, skip).
- **Corrections** : plusieurs crashs (toolbar sur image fixe, changement de langue,
  boîtes de message, bornes d'écran), artefacts d'affichage.
- **Icône** : l'exécutable standalone utilise l'icône Native Forge
  (`icons/nativeforge.ico`). L'icône d'origine du jeu n'est **pas** redistribuée.

Le détail se lit dans l'historique de la branche `versailles-fork` (un commit par
changement).

## Pourquoi cette version plutôt que ScummVM ?

ScummVM fait déjà tourner Versailles via son moteur cryomni3d — et, pour être clair,
**ni ScummVM ni cette version ne sont de l'émulation** : les deux exécutent le jeu en
code natif qui lit tes données d'origine. Ce projet est une *édition spécialisée et
packagée* de ce moteur, pour ceux qui veulent juste jouer à ce jeu sans rien configurer :

- **Un double-clic, zéro configuration** : un seul installeur lit ton CD/ISO et met tout
  en place sous forme d'appli autonome « Versailles » — pas d'installation de ScummVM,
  pas d'ajout de jeu, pas de fichiers à localiser.
- **Écran large 16:9** avec barres letterbox adaptatives — *absent de ScummVM standard*.
- **Bascule de langue en jeu** (FR/EN/DE/ZH), voix et textes séparés — *absent de
  ScummVM standard*.
- **Réglages taillés pour ce jeu** : plein écran, upscale lissé, keymap manette dédié,
  persistance des réglages.

Tout ce que le moteur fait déjà revient à l'équipe ScummVM. Si tu veux une solution
multi-plateforme, multi-jeux et activement maintenue, utilise ScummVM. Si tu veux une
édition native Windows de *Versailles 1685* prête à jouer, avec écran large et
changement de langue en jeu, utilise celle-ci. Les correctifs de ce fork pourront être
proposés en amont (upstream).

## Compiler

Build standalone Windows via MSYS2 / MinGW-w64, avec le moteur cryomni3d et le drapeau
`VERSAILLES_STANDALONE`. Voir la documentation de build de ScummVM
([`doc/`](doc), `./configure --help`) pour la chaîne d'outils de base. Ce dépôt ne
fournit pas les données du jeu : elles doivent provenir de votre propre copie
(CD / image ISO) de *Versailles 1685*.

## Crédits

- **ScummVM Team** — moteur cryomni3d et framework ScummVM (voir [`AUTHORS`](AUTHORS)).
- **Cryo Interactive** — jeu *Versailles 1685* d'origine (© 1996).
- **Native Forge** — ce fork, l'édition native et l'empaquetage.

Soutenir Native Forge : https://ko-fi.com/nativeforge
