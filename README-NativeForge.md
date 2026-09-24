# Versailles 1685 — édition native (fork ScummVM cryomni3d) · Native Forge

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

Environ 1200 lignes sur ~24 fichiers, réparties ainsi :

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

## Compiler

Build standalone Windows via MSYS2 / MinGW-w64, avec le moteur cryomni3d et le drapeau
`VERSAILLES_STANDALONE`. Voir la documentation de build de ScummVM
([`doc/`](doc), `./configure --help`) pour la chaîne d'outils de base.

Ce dépôt ne fournit pas les données du jeu : elles doivent provenir de votre propre
copie (CD / image ISO) de *Versailles 1685*.

## Crédits

- **ScummVM Team** — moteur cryomni3d et framework ScummVM (voir [`AUTHORS`](AUTHORS)).
- **Cryo Interactive** — jeu *Versailles 1685* d'origine (© 1996).
- **Native Forge** — ce fork, l'édition native et l'empaquetage.

Soutenir Native Forge : https://ko-fi.com/nativeforge
