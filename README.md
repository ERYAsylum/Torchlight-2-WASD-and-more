# TL2_WASD_and_more v1.0.02z1 - Steam

## English

Unofficial DLL mod for the Steam build of Torchlight II. It adds camera-relative keyboard movement, disables click-to-move without breaking combat/interactions, and includes an optional active range x1.5 engine patch.

Author: ERY_Asylum

### License and Community Policy

License: `GPL-3.0-or-later`

This mod is intended to stay open and community-driven.

- Streaming, recording, reviewing, showcasing, and monetizing gameplay footage using this mod on Twitch, YouTube, or similar platforms is explicitly allowed.
- Forks, ports, patches, and modified builds are welcome.
- If a modified version is distributed, it must remain open source under `GPL-3.0-or-later` and provide the corresponding source code.
- Do not distribute closed-source forks or proprietary paid versions of this mod.
- Do not present unofficial forks, builds, or ports as official releases.
- The project name `TL2_WASD_and_more` and official release labels should not be used to imply endorsement of unofficial versions.

Torchlight II is not included. This project is not affiliated with or endorsed by the owners of Torchlight II.

### Compatibility

This branch targets the Steam executable:

- Game: Torchlight II
- Build: Steam
- Executable version observed during development: `1.25.5.6`
- Expected install directory example: `C:\SteamLibrary\steamapps\common\Torchlight II`

Do not use this Steam build on the GOG executable. The Steam and GOG executables have different code layouts and require different hook addresses.

### Features

- Camera-relative keyboard movement.
- Automatic keyboard layout preset based on Windows keyboard layout detection:
  - Expected to work with any keyboard layout correctly reported by Windows.
  - Automatically maps to ZQSD on French AZERTY and to WASD-style physical movement on common QWERTY/QWERTZ layouts.
- Optional arrows/custom bindings through `TL2TrueKeyboardMove.ini` or the overlay custom profile flow.
- Click-to-move blocker that preserves attacks, pickups, aiming, menus and interactions.
- Hold-position key, default `Shift`.
- Integrated overlay opened with `End`.
- D3D9 in-frame overlay mirror for OBS/Game Capture.
- Active range x1.5 patch, controlled by config and applied by the `MSWSOCK.dll` proxy at startup.
- Release-brake and context-safety logic to reduce unwanted movement after key release, transitions, skills, loading and player-object swaps.
- Interaction-facing reset so keyboard movement clears the last vanilla look-at target after NPC/object interaction.

### Installation

1. Close Torchlight II.
2. Download the Steam binary release zip.
3. Extract or copy the three mod files from that zip into the Torchlight II directory next to `Torchlight2.exe`:
   - `TL2TrueKeyboardMove.dll`
   - `TL2TrueKeyboardMove.ini`
   - `MSWSOCK.dll`
4. Start Torchlight II normally.

To uninstall, remove those three files from the game directory.

### Default Controls

- `WASD` / `ZQSD`: move, depending on detected keyboard layout.
- `Shift`: hold position.
- `End`: open or close the overlay.
- `F5`: cycle overlay text scale while the overlay is open.
- `F6`: toggle the keyboard/mouse movement patch while the overlay is open.
- `F7`: toggle left-click movement while the overlay is open.
- `F8`: toggle active range x1.5 while the overlay is open.
- `F9`: recalibrate the camera movement basis while the overlay is open.
- `F10`: force the bounded emergency WASD bootstrap.
- `Delete`: start the custom profile binding flow while the overlay is open.

### Configuration

The main config file is `TL2TrueKeyboardMove.ini`.

Useful defaults:

- `[Keys] Preset=AUTO`
- `[KeyboardMousePatch] EnableKeyboardMousePatch=1`
- `[Mouse] DisableClickMovement=1`
- `[EnginePatch] EnableActiveRangePatch=1`
- `[Overlay] EnableD3D9FrameOverlay=1`

Changing `EnableActiveRangePatch` requires a full game restart because the engine patch is applied during startup by `MSWSOCK.dll`.

### Building From Source

Requirements:

- Visual Studio 2022 or Build Tools 2022.
- Desktop development with C++.
- MSVC v143 toolset.
- Windows SDK.

Build:

```powershell
Get-ChildItem -Path . -Recurse -File | Unblock-File
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\build.ps1
```

`Unblock-File` is useful when the source archive was downloaded from the internet and Windows marked the files as blocked. The execution policy change only applies to the current PowerShell process.

The script builds Release Win32, runs the movement/overlay tests, and writes the ready-to-package files under `dist`.

### Runtime Files

- `TL2TrueKeyboardMove.dll`: main mod.
- `TL2TrueKeyboardMove.ini`: user configuration.
- `MSWSOCK.dll`: loader/proxy used to load the mod and apply the active range patch.
- `TL2TrueKeyboardMove.log`: runtime log, created in the game directory.
- `TL2TrueKeyboardMove.mswsockproxy.log`: proxy log, created in the game directory.

### Troubleshooting

If keyboard movement does not start after loading into a map:

- Open the overlay with `End`.
- Press `F10` once while the overlay is open.
- If mouse movement is disabled, press `F7` while the overlay is open to temporarily restore left-click movement.
- Move once with the mouse, then try keyboard movement again.
- Press `F7` again while the overlay is open if you want to disable left-click movement again.
- Check `TL2TrueKeyboardMove.log` for hook or context-capture messages.

If the overlay or active range toggle changes a setting:

- Close Torchlight II completely.
- Start the game again.

Known overlay focus issue:

- In windowed fullscreen, `Enter` or `Escape` may not close the overlay if the Torchlight II window no longer has input focus.
- Click once inside the `Torchlight2.exe` game window, then press `Enter` or `Escape` again.

If the game crashes:

- Remove `MSWSOCK.dll` and `TL2TrueKeyboardMove.dll` to confirm the game starts unmodded.
- Restore the backed-up files if another mod also used `MSWSOCK.dll`.
- Keep the latest `TL2TrueKeyboardMove.log`, `TL2TrueKeyboardMove.mswsockproxy.log`, and `Ogre.log` for debugging.

### Warning

This is an unofficial binary mod that hooks Torchlight II at runtime. Use it at your own risk. Keep backups before installing, especially if other DLL mods are already present.

## Français

Mod DLL non officiel pour la version Steam de Torchlight II. Il ajoute un déplacement clavier relatif à la caméra, désactive le click-to-move sans casser les combats/interactions, et inclut un patch optionnel de portée active x1.5.

Auteur : ERY_Asylum

### Licence et politique communautaire

Licence : `GPL-3.0-or-later`

Ce mod est pensé pour rester ouvert et porté par la communauté.

- Le streaming, l'enregistrement, les reviews, les démonstrations et la monétisation de vidéos de gameplay utilisant ce mod sur Twitch, YouTube ou plateformes similaires sont explicitement autorisés.
- Les forks, ports, patchs et builds modifiés sont bienvenus.
- Toute version modifiée distribuée doit rester open source sous `GPL-3.0-or-later` et fournir le code source correspondant.
- Ne distribuez pas de forks fermés ni de versions propriétaires payantes de ce mod.
- Ne présentez pas des forks, builds ou ports non officiels comme des releases officielles.
- Le nom de projet `TL2_WASD_and_more` et les labels de release officiels ne doivent pas être utilisés pour laisser croire qu'une version non officielle est approuvée.

Torchlight II n'est pas inclus. Ce projet n'est pas affilié aux détenteurs de Torchlight II et n'est pas approuvé par eux.

### Compatibilité

Cette branche cible l'exécutable Steam :

- Jeu : Torchlight II
- Version : Steam
- Version d'exécutable observée pendant le développement : `1.25.5.6`
- Exemple de dossier d'installation attendu : `C:\SteamLibrary\steamapps\common\Torchlight II`

N'utilisez pas cette version Steam sur l'exécutable GOG. Les exécutables Steam et GOG ont une disposition mémoire différente et demandent des adresses de hooks différentes.

### Fonctionnalités

- Déplacement clavier relatif à la caméra.
- Préréglage automatique basé sur la détection de disposition clavier de Windows :
  - Devrait fonctionner avec toute disposition clavier correctement reconnue par Windows.
  - Bascule automatiquement en ZQSD sur AZERTY français et en déplacement physique de type WASD sur les dispositions QWERTY/QWERTZ courantes.
- Flèches ou touches personnalisées via `TL2TrueKeyboardMove.ini` ou via le profil custom de l'overlay.
- Blocage du click-to-move tout en conservant attaques, ramassage, visée, menus et interactions.
- Touche de maintien de position, `Shift` par défaut.
- Overlay intégré ouvert avec `End`.
- Miroir overlay D3D9 dans l'image du jeu pour OBS/Game Capture.
- Patch de portée active x1.5, contrôlé par la configuration et appliqué au démarrage par le proxy `MSWSOCK.dll`.
- Logique de freinage au relâchement et protections de contexte pour limiter les mouvements indésirables après relâchement, transitions, compétences, chargements et remplacements de joueur.
- Reset de l'orientation d'interaction pour que le déplacement clavier efface la dernière cible vanilla regardée après une interaction PNJ/objet.

### Installation

1. Fermez Torchlight II.
2. Téléchargez le zip binaire de la release Steam.
3. Extrayez ou copiez les trois fichiers du mod depuis ce zip dans le dossier de Torchlight II, à côté de `Torchlight2.exe` :
   - `TL2TrueKeyboardMove.dll`
   - `TL2TrueKeyboardMove.ini`
   - `MSWSOCK.dll`
4. Lancez Torchlight II normalement.

Pour désinstaller, supprimez ces trois fichiers du dossier du jeu.

### Contrôles par défaut

- `WASD` / `ZQSD` : déplacement, selon la disposition clavier détectée.
- `Shift` : maintien de position.
- `End` : ouvrir ou fermer l'overlay.
- `F5` : changer l'échelle du texte de l'overlay quand il est ouvert.
- `F6` : activer/désactiver le patch clavier/souris quand l'overlay est ouvert.
- `F7` : activer/désactiver le déplacement au clic gauche quand l'overlay est ouvert.
- `F8` : activer/désactiver la portée active x1.5 quand l'overlay est ouvert.
- `F9` : recalibrer la base de déplacement caméra quand l'overlay est ouvert.
- `F10` : forcer le bootstrap WASD d'urgence borné.
- `Delete` : lancer la configuration du profil custom quand l'overlay est ouvert.

### Configuration

Le fichier principal est `TL2TrueKeyboardMove.ini`.

Réglages utiles par défaut :

- `[Keys] Preset=AUTO`
- `[KeyboardMousePatch] EnableKeyboardMousePatch=1`
- `[Mouse] DisableClickMovement=1`
- `[EnginePatch] EnableActiveRangePatch=1`
- `[Overlay] EnableD3D9FrameOverlay=1`

Changer `EnableActiveRangePatch` nécessite un redémarrage complet du jeu, car le patch moteur est appliqué au démarrage par `MSWSOCK.dll`.

### Compilation depuis les sources

Prérequis :

- Visual Studio 2022 ou Build Tools 2022.
- Charge de travail Desktop development with C++.
- Toolset MSVC v143.
- Windows SDK.

Compilation :

```powershell
Get-ChildItem -Path . -Recurse -File | Unblock-File
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\build.ps1
```

`Unblock-File` est utile si l'archive des sources a été téléchargée depuis internet et que Windows a marqué les fichiers comme bloqués. Le changement de politique d'exécution ne vaut que pour le processus PowerShell courant.

Le script compile en Release Win32, lance les tests de mouvement/overlay, puis place les fichiers prêts à empaqueter dans `dist`.

### Fichiers à l'exécution

- `TL2TrueKeyboardMove.dll` : mod principal.
- `TL2TrueKeyboardMove.ini` : configuration utilisateur.
- `MSWSOCK.dll` : proxy/loader utilisé pour charger le mod et appliquer le patch de portée active.
- `TL2TrueKeyboardMove.log` : log runtime, créé dans le dossier du jeu.
- `TL2TrueKeyboardMove.mswsockproxy.log` : log du proxy, créé dans le dossier du jeu.

### Dépannage

Si le déplacement clavier ne démarre pas après le chargement d'une carte :

- Ouvrez l'overlay avec `End`.
- Appuyez une fois sur `F10` pendant que l'overlay est ouvert.
- Si le mouvement souris est désactivé, appuyez sur `F7` pendant que l'overlay est ouvert pour rétablir temporairement le déplacement au clic gauche.
- Faites un déplacement souris, puis réessayez le clavier.
- Appuyez de nouveau sur `F7` pendant que l'overlay est ouvert si vous voulez redésactiver le déplacement au clic gauche.
- Consultez `TL2TrueKeyboardMove.log` pour les messages de hook ou de capture de contexte.

Si l'overlay ou le toggle de portée active modifie un réglage :

- Fermez complètement Torchlight II.
- Relancez le jeu.

Problème de focus connu avec l'overlay :

- En plein écran fenêtré, `Enter` ou `Escape` peuvent ne pas fermer l'overlay si la fenêtre de Torchlight II n'a plus le focus clavier.
- Cliquez une fois dans la fenêtre du jeu `Torchlight2.exe`, puis réappuyez sur `Enter` ou `Escape`.

En cas de crash :

- Supprimez `MSWSOCK.dll` et `TL2TrueKeyboardMove.dll` pour vérifier que le jeu démarre sans mod.
- Restaurez les fichiers sauvegardés si un autre mod utilisait aussi `MSWSOCK.dll`.
- Conservez les derniers `TL2TrueKeyboardMove.log`, `TL2TrueKeyboardMove.mswsockproxy.log` et `Ogre.log` pour le diagnostic.

### Avertissement

Ceci est un mod binaire non officiel qui hook Torchlight II à l'exécution. Utilisez-le à vos risques. Gardez des sauvegardes avant installation, surtout si d'autres mods DLL sont déjà présents.
