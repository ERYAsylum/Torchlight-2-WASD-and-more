# TL2TrueControlerSupportAddon

Experimental controller addon for **Torchlight II**.

This addon is designed to run on top of the main **TL2 WASD and more / TL2TrueKeyboardMove** mod. It is not a replacement for the main mod.

## English

### Status

- Version: `v0.0.9`
- State: experimental
- Tested on: Windows 10
- Tested controller: Xbox controller
- Game: Torchlight II, Windows version
- Loader: optional `d3d9.dll` proxy addon
- Main mod required: yes

### Required Main Mod

This addon depends on the main **TL2 WASD and more / TL2TrueKeyboardMove** mod.

The main mod is required because the addon reuses its existing keyboard movement profile and sends the same movement intentions that the stable keyboard/mouse mod already understands.

You must install the correct main mod dependency for your game version:

- use the [Steam version of the main mod](https://github.com/ERYAsylum/Torchlight-2-WASD-and-more/releases/tag/steam-v1.0.02z1-beta1) if you play the Steam release of Torchlight II;
- use the [GOG version of the main mod](https://github.com/ERYAsylum/Torchlight-2-WASD-and-more/releases/tag/gog-v1.0.02z1-beta1) if you play the GOG release of Torchlight II.

This addon does not replace that Steam/GOG dependency and does not merge both game versions into one build.

The addon does **not** rewrite the stable WASD movement logic. It adds a controller input layer above it:

- the left stick sends the movement keys already configured by the main mod;
- gamepad buttons send the same keyboard or mouse inputs used by Torchlight II;
- Torchlight II actions can be resolved from the player's own `KeyBindings.dat`;
- the right stick controls the real Windows mouse cursor.

If the main mod is missing, the controller addon may still send some generic inputs, but the intended WASD/ZQSD camera-relative movement support will not work correctly.

### Important Warning

This is an experimental addon. It has only been tested on Windows 10 with an Xbox controller.

Use it as a test build. Keep a backup of your current files and configuration.

The addon uses input injection and a `d3d9.dll` proxy loader. This is common for small game addons, but it can conflict with other overlays, wrappers, injectors, or DirectX 9 mods.

### Input Backends

The addon uses several Windows input paths:

- **XInput** is used to read the controller state.
- **DirectInput** is used as a best-effort exclusive focus lock while Torchlight II is the active foreground window.
- **SendInput** is used to send keyboard, mouse button, and mouse movement events to the game.
- **Steam Input** can work if it exposes the controller as an XInput device, but it is not the preferred setup.

Recommended setup:

- If your Xbox controller works natively in Windows, disable Steam Input for Torchlight II.
- If your controller is not detected without Steam Input, Steam Input may be required on your setup.

Why Steam Input is not recommended when avoidable:

- it can keep some authority over the controller after `Alt+Tab`;
- it can continue to map the controller to desktop mouse/keyboard behavior;
- it may add latency, duplicate inputs, or instability when the game loses focus;
- it can make focus-loss behavior harder to control.

The DirectInput exclusive lock used by this addon is intended to avoid that problem: the addon tries to acquire the controller only while Torchlight II has focus and releases it when focus is lost.

On the tested setup, the addon works with the controller exposed directly to Windows/XInput and with the DirectInput focus lock enabled. With special controllers such as PlayStation, Switch Pro, or generic controllers, you may need an external tool or driver that exposes the controller as XInput or DirectInput.

Steam also provides advanced support for non-Xbox controllers. If your controller cannot be detected reliably through native Windows/XInput/DirectInput paths, Steam Input can be used as a fallback to test, especially for PlayStation, Switch Pro, or generic controllers. If you use that fallback, re-check focus behavior after `Alt+Tab`.

### Keyboard/Mouse And Controller Switching

The addon is not meant to force a controller-only mode. Keyboard/mouse and controller controls can be alternated on the fly during gameplay.

Recommended recovery settings are enabled by default:

```ini
[Recovery]
EnablePhysicalKeyboardMouseOverride=1
EnableBackStartReleaseCombo=1
```

With these settings:

- moving the physical mouse or pressing the physical keyboard temporarily releases controller-owned inputs;
- normal keyboard/mouse control can be resumed on the fly without closing the game;
- controller input resumes automatically after the short recovery delay;
- holding `Back + Start` releases all controller-owned inputs and briefly suspends controller injection;
- losing focus releases controller-owned inputs when `FocusRequired=1`.

This is useful if the right stick mouse control, a held movement direction, or a gamepad button ever interferes with normal keyboard/mouse control.

### Features

- Optional addon above the stable keyboard/mouse mod.
- Does not modify `TL2TrueKeyboardMove.dll`.
- Does not modify the Steam/GOG offsets of the main mod.
- Loads independently through `d3d9.dll`.
- Activates only inside `Torchlight2.exe`, not inside the launcher.
- Left stick movement mapped to the current keyboard movement profile.
- Supports WASD, ZQSD, AZERTY, QWERTY, and custom movement profiles through the main mod configuration.
- Right stick controls the real mouse cursor.
- Right stick default mode: relative mouse movement.
- R3 toggles relative mouse mode and radial aim mode.
- Radial aim mode places the real cursor around the center of the game window.
- Radial aim can keep the last direction on release.
- Configurable deadzones, sensitivity, radial radius, radial center offset, and radial hysteresis.
- Buttons can send keyboard keys, mouse buttons, Torchlight II actions, movement directions, or gamepad chords.
- Reads the current user's `KeyBindings.dat` automatically when possible.
- Supports custom and localized key bindings.
- Includes an English Windows configuration tool.
- Suspends controller injection when Torchlight II loses focus.
- Releases controller-owned inputs when focus is lost or when the controller disconnects.
- Recovery option: physical keyboard/mouse input temporarily takes back control.
- Recovery option: hold `Back + Start` to release controller-owned input.

### Installation

Do **not** download the GitHub `Source code` archive if you only want to play.

Download the addon **release archive** instead. The release archive contains only the files needed to use the addon:

- `d3d9.dll`
- `TL2TrueControlerSupport.ini`
- `TL2TrueControlerSupportConfig.exe`

1. Install and verify the main **TL2 WASD and more / TL2TrueKeyboardMove** mod first.
2. Make sure the main mod works correctly with keyboard and mouse.
3. Copy these addon files next to `Torchlight2.exe`:
   - `d3d9.dll`
   - `TL2TrueControlerSupport.ini`
   - `TL2TrueControlerSupportConfig.exe`
4. Keep `TL2TrueKeyboardMove.ini` next to the game executable as required by the main mod.
5. Optional: run `TL2TrueControlerSupportConfig.exe` before launching the game if you want to review or customize the controller bindings.
6. Close `TL2TrueControlerSupportConfig.exe` before launching Torchlight II.
7. Launch Torchlight II normally.

The addon is loaded by the game process through `d3d9.dll`. The launcher may appear first, but the addon only activates inside `Torchlight2.exe`.

Do not keep the configuration tool open while launching or playing Torchlight II. The configurator can acquire the controller for capture/configuration, and leaving it open while the game starts can cause input bugs or controller focus conflicts.

### Default Controls

The default profile uses Torchlight II action names such as `Game:QK1`, not hardcoded keyboard keys. This lets the addon follow the current player's customized key bindings when possible.

| Gamepad input | Default action |
| --- | --- |
| Left stick | Movement directions from the main mod profile |
| Right stick | Real mouse cursor movement |
| R3 | Toggle relative mouse / radial aim |
| A | Move/Attack |
| B | Cast Active Skill |
| X | Quickslot 5 |
| Y | Quickslot 6 |
| LB | Weapon Swap |
| Back | Automap |
| Start | Options Menu |
| L3 | Hold Position |
| D-pad Left | Quickslot 1 |
| D-pad Right | Quickslot 2 |
| D-pad Up | Quickslot 3 |
| D-pad Down | Quickslot 4 |
| RT | Quickslot 8 |
| LT + A | Quickslot 9 |
| LT + B | Quickslot 0 |
| RB + D-pad Up | Skill Menu |
| RB + D-pad Right | Inventory |
| RB + D-pad Down | Quest Menu |
| RB + D-pad Left | Pet Inventory |
| RB + B | Close All Menus |
| RB + A | Show Items |

Quickslots are generic Torchlight II slots. They can contain skills, potions, scrolls, or any other action/item assigned by the player.

### Configuration

Run `TL2TrueControlerSupportConfig.exe` to edit the gamepad mappings.

The configurator:

- opens a real Windows window;
- displays Torchlight II actions on the left;
- displays the assigned gamepad bind on the right;
- supports scrolling and resizing;
- supports single button capture;
- supports combo capture with `LB`, `RB`, `LT`, `RT`;
- supports skipping actions;
- can clear a bind;
- can restore the default profile;
- writes the result into `TL2TrueControlerSupport.ini`.

Supported examples:

```ini
A=Game:MOVE/ATTACK
B=Game:CASTSKILL
X=Game:QK5
RightThumb=Addon:ToggleRightStickMouseMode
LeftTrigger+A=Game:QK9
RightShoulder+DPadUp=Game:SKILLMENU
```

Action values can be:

- `None`
- `MouseLeft`, `MouseRight`, `MouseMiddle`, `MouseX1`, `MouseX2`
- keyboard names such as `A`, `Space`, `Tab`, `Escape`, `Shift`, `Ctrl`, `F1`
- key combos such as `Ctrl+F`, `Shift+Tab`, `Alt+Enter`
- Torchlight II actions such as `Game:QK1`, `Game:INVENTORYMENU`, `Game:MOVE/ATTACK`
- movement directions such as `Move:Up`, `Move:Down`, `Move:Left`, `Move:Right`

### User Key Bindings

The addon tries to find the current user's Torchlight II bindings automatically:

```ini
KeyBindingsDat=AUTO
```

Automatic search checks:

```text
Documents\My Games\runic games\torchlight 2\KeyBindings.dat
Documents\My Games\runic games\torchlight 2\save\*\KeyBindings.dat
```

This is important because the save folder can vary for every user.

The addon resolves `Game:ACTIONNAME` through the active user bindings first, then falls back to `settings.txt` when needed.

### Right Stick Mouse Modes

The right stick always controls the real mouse cursor.

Default mode:

- relative mouse movement;
- behaves like moving the physical mouse;
- configured with `SensitivityX`, `SensitivityY`, and `Deadzone`.

Radial aim mode:

- toggled with R3 by default;
- places the cursor around the game window center;
- useful for quickly aiming skills around the character;
- keeps the last aimed direction on release by default;
- uses radial-only activation/release thresholds to avoid stick snapback changing direction.

Recommended radial settings:

```ini
[Mouse]
RadialRadius=140
RadialCenterOffsetX=0
RadialCenterOffsetY=0
RadialReturnToCenter=0
RadialUseFixedRadius=1
RadialActivationThresholdX1000=450
RadialReleaseThresholdX1000=300
```

Tuning notes:

- Increase `RadialRadius` to aim farther from the character.
- Decrease `RadialRadius` to aim closer to the character.
- Keep `RadialReturnToCenter=0` if you want the cursor to stay on the last direction after release.
- Set `RadialReturnToCenter=1` only if you want the cursor to snap back to the center.
- Keep `RadialUseFixedRadius=1` if you want the cursor to stick to the outer aiming ring.
- Increase `RadialActivationThresholdX1000` if accidental aim changes happen too easily.
- Increase `RadialReleaseThresholdX1000` if the aim should stop sooner when releasing the stick.

### Focus And Recovery

Recommended settings:

```ini
[General]
FocusRequired=1
AcquireControllerExclusive=1

[Recovery]
EnablePhysicalKeyboardMouseOverride=1
EnableBackStartReleaseCombo=1
```

With these settings:

- controller input is suspended when Torchlight II loses focus;
- controller-owned inputs are released on focus loss;
- the addon attempts to acquire the controller while the game has focus;
- the addon releases the controller when the game loses focus;
- physical keyboard/mouse input can temporarily take control back;
- holding `Back + Start` releases controller-owned input and briefly suspends injection.

### Troubleshooting

#### The addon does not load

- Make sure `d3d9.dll` is next to `Torchlight2.exe`.
- Make sure the game is using DirectX 9.
- Check for another `d3d9.dll` wrapper or overlay conflict.
- Check `TL2TrueControlerSupport.log` if it exists next to the game executable.
- Remember that the launcher starts before the game; the addon activates only inside `Torchlight2.exe`.

#### The controller is not detected

- Try with Steam Input disabled first.
- If the controller is not visible without Steam Input, enable Steam Input and test again.
- Verify that Windows detects the controller.
- Test with an Xbox controller first if possible.
- For PlayStation, Switch Pro, or generic controllers, use a driver/tool that exposes the controller as XInput or DirectInput if needed.
- Steam Input can be used as a fallback for exotic controllers, but focus behavior should be tested carefully.
- Make sure no other program is taking exclusive control of the controller.

#### The controller controls the desktop after Alt+Tab

- Disable Steam Input for Torchlight II if possible.
- Keep `FocusRequired=1`.
- Keep `AcquireControllerExclusive=1`.
- Close tools that map the controller to desktop mouse/keyboard input.
- If the issue happens only with Steam Input, use native Xbox/XInput mode instead.

#### Movement works but buttons do not match my bindings

- Run `TL2TrueControlerSupportConfig.exe`.
- Use `Game:ACTIONNAME` bindings instead of hardcoded keyboard keys when possible.
- Keep `KeyBindingsDat=AUTO`.
- Make sure the correct Torchlight II user save folder is being detected.
- Re-save your controls from the Torchlight II options menu, then run the configurator again.

#### The right stick feels too sensitive

- Increase `[Mouse] Deadzone`.
- Lower `SensitivityX` and `SensitivityY` for relative mode.
- In radial mode, adjust `RadialRadius`.
- Increase `RadialActivationThresholdX1000` if accidental radial direction changes happen.

#### Keyboard/mouse should take control back

- Move the physical mouse or press a physical keyboard key.
- Hold `Back + Start` on the controller to release controller-owned input.
- Set `EnablePhysicalKeyboardMouseOverride=1`.

### Build From Source

Requirements:

- Visual Studio 2022 or Build Tools 2022
- Desktop development with C++
- MSVC v143
- Windows SDK

Build:

```powershell
.\build.ps1
```

Compiled files are written to `dist`.

### Compatibility Notes

- Designed for Windows.
- Tested only on Windows 10.
- Tested only with an Xbox controller.
- Steam and GOG game builds should remain separated at the main mod level when needed.
- This addon should not require a new Torchlight II character.
- This addon does not replace Torchlight II mod files.
- This addon does not edit save files.
- This addon does not patch the main mod offsets.
- This addon should normally be compatible with classic Torchlight II modloader mods, because it is a DLL-based input addon and does not modify the usual `.mod` game data logic.
- Conflicts are still possible with other DLL injectors, DirectX wrappers, overlays, or tools that also hook input or `d3d9.dll`.
- It may be incompatible with ReShade or any other mod/tool that also needs its own `d3d9.dll` next to `Torchlight2.exe`, unless a compatible DLL chaining setup is created manually.

### License

This addon is distributed under the **GNU General Public License v3.0**.

In short:

- you may use, study, modify, and redistribute this addon;
- if you redistribute modified versions, you must provide the corresponding source code under the same GPLv3 license;
- the addon is provided without warranty;
- keep the license notice and copyright notices when redistributing.

For GitHub publication, include a `LICENSE` file containing the full GPLv3 text, or set the repository license to `GPL-3.0`.

This addon is intended as an optional companion for the main Torchlight II WASD movement mod. It does not include Torchlight II game files.

---

## Francais

Addon experimental de support manette pour **Torchlight II**.

Cet addon est concu pour fonctionner au-dessus du mod principal **TL2 WASD and more / TL2TrueKeyboardMove**. Il ne remplace pas le mod principal.

### Statut

- Version : `v0.0.9`
- Etat : experimental
- Teste sur : Windows 10
- Manette testee : manette Xbox
- Jeu : Torchlight II, version Windows
- Chargement : addon optionnel via proxy `d3d9.dll`
- Mod principal requis : oui

### Mod Principal Obligatoire

Cet addon depend du mod principal **TL2 WASD and more / TL2TrueKeyboardMove**.

Le mod principal est indispensable, car l'addon reutilise son profil de deplacement clavier et envoie les memes intentions de mouvement que le mod clavier/souris stable comprend deja.

Vous devez installer la bonne dependance du mod principal selon votre version du jeu :

- utilisez la [version Steam du mod principal](https://github.com/ERYAsylum/Torchlight-2-WASD-and-more/releases/tag/steam-v1.0.02z1-beta1) si vous jouez a la version Steam de Torchlight II ;
- utilisez la [version GOG du mod principal](https://github.com/ERYAsylum/Torchlight-2-WASD-and-more/releases/tag/gog-v1.0.02z1-beta1) si vous jouez a la version GOG de Torchlight II.

Cet addon ne remplace pas cette dependance Steam/GOG et ne fusionne pas les deux versions du jeu dans un seul build.

L'addon ne reecrit pas la logique stable du mouvement WASD/ZQSD. Il ajoute une couche d'entree manette au-dessus :

- le stick gauche envoie les touches de mouvement deja configurees par le mod principal ;
- les boutons de la manette envoient les memes entrees clavier ou souris que Torchlight II ;
- les actions Torchlight II peuvent etre resolues depuis le `KeyBindings.dat` du joueur ;
- le stick droit controle le vrai curseur souris Windows.

Si le mod principal est absent, l'addon peut encore envoyer certaines entrees generiques, mais le deplacement camera-relative WASD/ZQSD prevu ne fonctionnera pas correctement.

### Avertissement Important

Cet addon est experimental. Il a seulement ete teste sur Windows 10 avec une manette Xbox.

Utilisez-le comme une version de test. Gardez une copie de vos fichiers et de votre configuration actuelle.

L'addon utilise de l'injection d'entrees et un proxy `d3d9.dll`. C'est courant pour de petits addons de jeu, mais cela peut entrer en conflit avec d'autres overlays, wrappers, injecteurs ou mods DirectX 9.

### APIs Et Entrees Utilisees

L'addon utilise plusieurs chemins d'entree Windows :

- **XInput** sert a lire l'etat de la manette.
- **DirectInput** sert a tenter une reservation exclusive de la manette pendant que Torchlight II est la fenetre active.
- **SendInput** sert a envoyer les entrees clavier, boutons souris et mouvements de souris au jeu.
- **Steam Input** peut fonctionner s'il expose la manette comme peripherique XInput, mais ce n'est pas la configuration recommandee.

Configuration recommandee :

- Si votre manette Xbox fonctionne nativement dans Windows, desactivez Steam Input pour Torchlight II.
- Si votre manette n'est pas detectee sans Steam Input, Steam Input peut etre necessaire sur votre machine.

Pourquoi Steam Input n'est pas recommande quand on peut l'eviter :

- il peut garder une autorite sur la manette apres un `Alt+Tab` ;
- il peut continuer a transformer la manette en souris/clavier de bureau ;
- il peut ajouter de la latence, doubler certaines entrees ou creer de l'instabilite quand le jeu perd le focus ;
- il rend le comportement en perte de focus plus difficile a maitriser.

Le verrou exclusif DirectInput utilise par cet addon vise a eviter ce probleme : l'addon tente de prendre la manette seulement pendant que Torchlight II a le focus, puis la libere quand le focus est perdu.

Sur la configuration testee, l'addon fonctionne avec la manette exposee directement a Windows/XInput et avec le verrou de focus DirectInput active. Avec des manettes particulieres comme PlayStation, Switch Pro ou des manettes generiques, il faudra peut-etre utiliser un outil externe ou un pilote qui expose la manette en XInput ou DirectInput.

Steam propose aussi une prise en charge avancee des manettes non-Xbox. Si votre manette n'est pas detectee de facon fiable par les chemins natifs Windows/XInput/DirectInput, Steam Input peut servir de solution de repli a tester, surtout pour les manettes PlayStation, Switch Pro ou generiques. Si vous utilisez cette solution de repli, retestez soigneusement le comportement en perte de focus apres `Alt+Tab`.

### Alternance Clavier/Souris Et Manette

L'addon n'impose pas un mode exclusivement manette. Il est possible d'alterner a la volee entre le clavier/souris et la manette pendant la partie.

Les reglages de recuperation recommandes sont actives par defaut :

```ini
[Recovery]
EnablePhysicalKeyboardMouseOverride=1
EnableBackStartReleaseCombo=1
```

Avec ces reglages :

- bouger la souris physique ou appuyer sur une touche du clavier physique libere temporairement les entrees controlees par la manette ;
- le controle clavier/souris normal peut etre repris a la volee sans fermer le jeu ;
- les entrees manette reprennent automatiquement apres le court delai de recuperation ;
- maintenir `Back + Start` libere toutes les entrees controlees par la manette et suspend brievement l'injection ;
- la perte de focus libere les entrees controlees par la manette quand `FocusRequired=1`.

C'est utile si le controle souris du stick droit, une direction maintenue ou un bouton manette interfere avec le controle clavier/souris normal.

### Caracteristiques

- Addon optionnel au-dessus du mod clavier/souris stable.
- Ne modifie pas `TL2TrueKeyboardMove.dll`.
- Ne modifie pas les offsets Steam/GOG du mod principal.
- Se charge independamment via `d3d9.dll`.
- S'active uniquement dans `Torchlight2.exe`, pas dans le launcher.
- Le stick gauche utilise le profil de mouvement clavier actuel.
- Compatible WASD, ZQSD, AZERTY, QWERTY et profils de mouvement personnalises via la configuration du mod principal.
- Le stick droit controle le vrai curseur souris.
- Mode par defaut du stick droit : mouvement souris relatif.
- R3 alterne entre le mode souris relatif et le mode visee radiale.
- Le mode radial place le vrai curseur autour du centre de la fenetre du jeu.
- Le mode radial peut garder la derniere direction au relachement.
- Deadzones, sensibilite, rayon radial, decalage du centre et hysteresis radiale configurables.
- Les boutons peuvent envoyer des touches clavier, boutons souris, actions Torchlight II, directions de mouvement ou combinaisons manette.
- Lecture automatique du `KeyBindings.dat` de l'utilisateur quand possible.
- Support des touches personnalisees et localisees.
- Outil de configuration Windows en anglais inclus.
- Suspension des entrees manette quand Torchlight II perd le focus.
- Liberation des entrees controlees par la manette en perte de focus ou deconnexion.
- Recuperation : le clavier/souris physique peut reprendre temporairement le controle.
- Recuperation : maintenir `Back + Start` libere les entrees controlees par la manette.

### Installation

Ne telechargez pas l'archive GitHub `Source code` si vous voulez seulement jouer.

Telechargez plutot l'archive **release** de l'addon. L'archive release contient uniquement les fichiers necessaires pour utiliser l'addon :

- `d3d9.dll`
- `TL2TrueControlerSupport.ini`
- `TL2TrueControlerSupportConfig.exe`

1. Installez et verifiez d'abord le mod principal **TL2 WASD and more / TL2TrueKeyboardMove**.
2. Assurez-vous que le mod principal fonctionne correctement au clavier/souris.
3. Copiez ces fichiers de l'addon a cote de `Torchlight2.exe` :
   - `d3d9.dll`
   - `TL2TrueControlerSupport.ini`
   - `TL2TrueControlerSupportConfig.exe`
4. Gardez `TL2TrueKeyboardMove.ini` a cote de l'executable du jeu, comme requis par le mod principal.
5. Optionnel : lancez `TL2TrueControlerSupportConfig.exe` avant de demarrer le jeu si vous voulez verifier ou personnaliser les raccourcis manette.
6. Fermez `TL2TrueControlerSupportConfig.exe` avant de lancer Torchlight II.
7. Lancez Torchlight II normalement.

L'addon est charge par le processus du jeu via `d3d9.dll`. Le launcher peut apparaitre avant le jeu, mais l'addon ne s'active que dans `Torchlight2.exe`.

Ne laissez pas l'outil de configuration ouvert pendant le lancement ou pendant une partie de Torchlight II. Le configurateur peut reserver la manette pour la capture/configuration, et le laisser ouvert pendant le demarrage du jeu peut provoquer des bugs d'entrees ou des conflits de focus manette.

### Controles Par Defaut

Le profil par defaut utilise les noms d'actions Torchlight II comme `Game:QK1`, pas des touches clavier codees en dur. Cela permet a l'addon de suivre les raccourcis personnalises du joueur quand possible.

| Entree manette | Action par defaut |
| --- | --- |
| Stick gauche | Directions de mouvement du profil du mod principal |
| Stick droit | Mouvement du vrai curseur souris |
| R3 | Alterner souris relative / visee radiale |
| A | Move/Attack |
| B | Cast Active Skill |
| X | Quickslot 5 |
| Y | Quickslot 6 |
| LB | Weapon Swap |
| Back | Automap |
| Start | Options Menu |
| L3 | Hold Position |
| D-pad gauche | Quickslot 1 |
| D-pad droite | Quickslot 2 |
| D-pad haut | Quickslot 3 |
| D-pad bas | Quickslot 4 |
| RT | Quickslot 8 |
| LT + A | Quickslot 9 |
| LT + B | Quickslot 0 |
| RB + D-pad haut | Skill Menu |
| RB + D-pad droite | Inventory |
| RB + D-pad bas | Quest Menu |
| RB + D-pad gauche | Pet Inventory |
| RB + B | Close All Menus |
| RB + A | Show Items |

Les quickslots sont des emplacements generiques de Torchlight II. Ils peuvent contenir competences, potions, parchemins ou toute autre action/objet assigne par le joueur.

### Configuration

Lancez `TL2TrueControlerSupportConfig.exe` pour modifier les raccourcis manette.

Le configurateur :

- ouvre une vraie fenetre Windows ;
- affiche les actions Torchlight II a gauche ;
- affiche le bind manette a droite ;
- supporte le defilement et le redimensionnement ;
- supporte la capture d'un bouton simple ;
- supporte la capture de combinaisons avec `LB`, `RB`, `LT`, `RT` ;
- permet de sauter des actions ;
- permet d'effacer un bind ;
- peut restaurer le profil par defaut ;
- ecrit le resultat dans `TL2TrueControlerSupport.ini`.

Exemples supportes :

```ini
A=Game:MOVE/ATTACK
B=Game:CASTSKILL
X=Game:QK5
RightThumb=Addon:ToggleRightStickMouseMode
LeftTrigger+A=Game:QK9
RightShoulder+DPadUp=Game:SKILLMENU
```

Les valeurs d'action peuvent etre :

- `None`
- `MouseLeft`, `MouseRight`, `MouseMiddle`, `MouseX1`, `MouseX2`
- noms de touches comme `A`, `Space`, `Tab`, `Escape`, `Shift`, `Ctrl`, `F1`
- combinaisons clavier comme `Ctrl+F`, `Shift+Tab`, `Alt+Enter`
- actions Torchlight II comme `Game:QK1`, `Game:INVENTORYMENU`, `Game:MOVE/ATTACK`
- directions de mouvement comme `Move:Up`, `Move:Down`, `Move:Left`, `Move:Right`

### Raccourcis Utilisateur

L'addon tente de trouver automatiquement les raccourcis Torchlight II de l'utilisateur actuel :

```ini
KeyBindingsDat=AUTO
```

La recherche automatique verifie :

```text
Documents\My Games\runic games\torchlight 2\KeyBindings.dat
Documents\My Games\runic games\torchlight 2\save\*\KeyBindings.dat
```

C'est important car le dossier de sauvegarde varie selon l'utilisateur.

L'addon resout `Game:ACTIONNAME` depuis les raccourcis actifs du joueur, puis utilise `settings.txt` en secours si necessaire.

### Modes Souris Du Stick Droit

Le stick droit controle toujours le vrai curseur souris.

Mode par defaut :

- mouvement souris relatif ;
- se comporte comme un mouvement de souris physique ;
- configurable avec `SensitivityX`, `SensitivityY` et `Deadzone`.

Mode visee radiale :

- active/desactive avec R3 par defaut ;
- place le curseur autour du centre de la fenetre du jeu ;
- utile pour orienter rapidement les competences autour du personnage ;
- garde la derniere direction visee au relachement par defaut ;
- utilise des seuils entree/sortie propres au radial pour eviter que le retour physique du stick change la direction.

Reglages radiaux recommandes :

```ini
[Mouse]
RadialRadius=140
RadialCenterOffsetX=0
RadialCenterOffsetY=0
RadialReturnToCenter=0
RadialUseFixedRadius=1
RadialActivationThresholdX1000=450
RadialReleaseThresholdX1000=300
```

Notes de reglage :

- Augmentez `RadialRadius` pour viser plus loin du personnage.
- Diminuez `RadialRadius` pour viser plus pres du personnage.
- Gardez `RadialReturnToCenter=0` si vous voulez garder la derniere direction au relachement.
- Mettez `RadialReturnToCenter=1` seulement si vous voulez un retour du curseur au centre.
- Gardez `RadialUseFixedRadius=1` si vous voulez que le curseur colle a l'anneau exterieur.
- Augmentez `RadialActivationThresholdX1000` si la direction change trop facilement par accident.
- Augmentez `RadialReleaseThresholdX1000` si la visee doit s'arreter plus tot au relachement du stick.

### Focus Et Recuperation

Reglages recommandes :

```ini
[General]
FocusRequired=1
AcquireControllerExclusive=1

[Recovery]
EnablePhysicalKeyboardMouseOverride=1
EnableBackStartReleaseCombo=1
```

Avec ces reglages :

- les entrees manette sont suspendues quand Torchlight II perd le focus ;
- les entrees controlees par la manette sont liberees en perte de focus ;
- l'addon tente de reserver la manette pendant que le jeu a le focus ;
- l'addon libere la manette quand le jeu perd le focus ;
- le clavier/souris physique peut reprendre temporairement le controle ;
- maintenir `Back + Start` libere les entrees controlees par la manette et suspend brievement l'injection.

### Depannage

#### L'addon ne se charge pas

- Verifiez que `d3d9.dll` est a cote de `Torchlight2.exe`.
- Verifiez que le jeu utilise DirectX 9.
- Verifiez qu'il n'y a pas un autre wrapper ou overlay `d3d9.dll` en conflit.
- Consultez `TL2TrueControlerSupport.log` s'il existe a cote de l'executable du jeu.
- Le launcher se lance avant le jeu ; l'addon ne s'active que dans `Torchlight2.exe`.

#### La manette n'est pas detectee

- Essayez d'abord avec Steam Input desactive.
- Si la manette n'est pas visible sans Steam Input, activez Steam Input et testez a nouveau.
- Verifiez que Windows detecte la manette.
- Testez avec une manette Xbox si possible.
- Pour les manettes PlayStation, Switch Pro ou generiques, utilisez si necessaire un pilote/outil qui expose la manette en XInput ou DirectInput.
- Steam Input peut servir de solution de repli pour les manettes exotiques, mais le comportement en perte de focus doit etre teste attentivement.
- Verifiez qu'aucun autre programme ne prend le controle exclusif de la manette.

#### La manette controle le bureau apres Alt+Tab

- Desactivez Steam Input pour Torchlight II si possible.
- Gardez `FocusRequired=1`.
- Gardez `AcquireControllerExclusive=1`.
- Fermez les outils qui transforment la manette en souris/clavier de bureau.
- Si le probleme arrive seulement avec Steam Input, utilisez le mode Xbox/XInput natif.

#### Le mouvement fonctionne mais les boutons ne correspondent pas a mes raccourcis

- Lancez `TL2TrueControlerSupportConfig.exe`.
- Utilisez les binds `Game:ACTIONNAME` plutot que des touches clavier codees en dur quand possible.
- Gardez `KeyBindingsDat=AUTO`.
- Verifiez que le bon dossier utilisateur Torchlight II est detecte.
- Re-sauvegardez vos controles depuis les options de Torchlight II, puis relancez le configurateur.

#### Le stick droit est trop sensible

- Augmentez `[Mouse] Deadzone`.
- Baissez `SensitivityX` et `SensitivityY` pour le mode relatif.
- En mode radial, ajustez `RadialRadius`.
- Augmentez `RadialActivationThresholdX1000` si la direction radiale change trop facilement.

#### Le clavier/souris doit reprendre le controle

- Bougez la souris physique ou appuyez sur une touche du clavier physique.
- Maintenez `Back + Start` sur la manette pour liberer les entrees controlees par la manette.
- Gardez `EnablePhysicalKeyboardMouseOverride=1`.

### Compilation Depuis Les Sources

Prerequis :

- Visual Studio 2022 ou Build Tools 2022
- Desktop development with C++
- MSVC v143
- Windows SDK

Compilation :

```powershell
.\build.ps1
```

Les fichiers compiles sont places dans `dist`.

### Notes De Compatibilite

- Concu pour Windows.
- Teste seulement sur Windows 10.
- Teste seulement avec une manette Xbox.
- Les versions Steam et GOG doivent rester separees au niveau du mod principal si necessaire.
- Cet addon ne devrait pas necessiter de nouveau personnage Torchlight II.
- Cet addon ne remplace pas les fichiers de mods Torchlight II.
- Cet addon ne modifie pas les sauvegardes.
- Cet addon ne patch pas les offsets du mod principal.
- Cet addon ne devrait normalement pas etre incompatible avec les mods classiques du modloader Torchlight II, car c'est un addon d'entree via DLL et il ne modifie pas la logique habituelle des donnees de jeu `.mod`.
- Des conflits restent possibles avec d'autres injecteurs DLL, wrappers DirectX, overlays ou outils qui hookent aussi les entrees ou `d3d9.dll`.
- Il peut etre incompatible avec ReShade ou tout autre mod/outil qui a aussi besoin de son propre `d3d9.dll` a cote de `Torchlight2.exe`, sauf si un chainage DLL compatible est cree manuellement.

### Licence

Cet addon est distribue sous la **GNU General Public License v3.0**.

En resume :

- vous pouvez utiliser, etudier, modifier et redistribuer cet addon ;
- si vous redistribuez des versions modifiees, vous devez fournir le code source correspondant sous la meme licence GPLv3 ;
- l'addon est fourni sans garantie ;
- conservez les mentions de licence et de copyright lors de la redistribution.

Pour une publication GitHub, ajoutez un fichier `LICENSE` contenant le texte complet de la GPLv3, ou configurez la licence du depot sur `GPL-3.0`.

Cet addon est concu comme compagnon optionnel du mod principal de deplacement WASD pour Torchlight II. Il n'inclut aucun fichier du jeu Torchlight II.
