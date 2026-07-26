<img width="1920" height="1080" alt="Capture d’écran (546)" src="https://github.com/user-attachments/assets/0d01e3e7-9e66-4ecf-a61f-ff3e8a562263" />

# Torchlight 2 WASD and more

Unofficial Torchlight II DLL mod adding camera-relative WASD/ZQSD keyboard movement, click-to-move control options, overlay configuration, and an optional active range x1.5 patch.

An experimental optional controller support addon is also available. It requires the main WASD mod and adds controller input on top of it, including left stick movement, right stick real mouse control, configurable buttons, and a separate configuration tool.

## Steam Deck / SteamOS

Community reports indicate that the mod works on Steam Deck, and the optional Controller Addon does not appear to be required.

Compatibility may vary depending on your SteamOS, Proton, or Wine configuration:

Some users reported that the mod works out of the box with no additional setup.
Others reported that they needed to add the following launch option in Steam:
WINEDLLOVERRIDES="MSWSOCK.dll=n,b" %command%

You can add this under:

Steam → Torchlight II → Properties → Launch Options

Since I do not own a Steam Deck or have a SteamOS development environment, I cannot officially test or support this platform. Community feedback is greatly appreciated and will help improve the documentation.

## Important Keybinding Notice

Before installing any component of this mod, it is recommended to set up your Torchlight II key bindings with the WASD/ZQSD movement scheme in mind. To avoid duplicate inputs or key overlaps, free the movement keys you plan to use for the mod and reassign any existing in-game actions that already use those keys.

## Download

Choose the release matching your game version.

- [Download Steam version](https://github.com/ERYAsylum/Torchlight-2-WASD-and-more/releases/tag/steam-v1.0.02z1-beta1)
- [Download GOG version](https://github.com/ERYAsylum/Torchlight-2-WASD-and-more/releases/tag/gog-v1.0.02z1-beta1)

Do not mix versions. The Steam and GOG executables use different internal addresses, so each build must be used only with its matching game version.

## Optional Controller Addon

- [Download TL2TrueControlerSupportAddon v0.0.9](https://github.com/ERYAsylum/Torchlight-2-WASD-and-more/releases/tag/TL2TrueControlerSupportAddonv0.0.9)

The controller addon is experimental and has only been tested on Windows 10 with an Xbox controller. It depends on the main mod, so install the correct Steam or GOG version of the main mod first.

## Source Branches

- Steam branch
- GOG branch
- TL2TrueControlerSupportAddon branch

## License

GPL-3.0-or-later.

Streaming, recording, reviewing, showcasing, and monetizing gameplay footage using this mod on Twitch, YouTube, or similar platforms is explicitly allowed.

Torchlight II is not included. This project is not affiliated with or endorsed by the owners of Torchlight II.

<img width="1254" height="1254" alt="tl2-wasd-and-more-logo" src="https://github.com/user-attachments/assets/8d6b5bac-774f-457b-bd78-e667a7c90a26" />
