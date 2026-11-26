# SMB3 Remastered PC

An experimental PC build for the original **Super Mario Bros. 3**, with widescreen, a remastered presentation, and keyboard and controller support. It runs the original campaign and alternating two-player mode using your own NES ROM.

I started this project because I wanted to play a remastered version of Mario 3 on a modern screen. It's a free, unofficial fan tribute to a game I love, made independently of Nintendo and with respect for the people who created the original. Contributions at any level are welcome.

## Download and play

Download the package for your platform from **Releases** on GitHub. For a development build, open a successful run under **Actions → Build and test without a ROM** and download the same package from **Artifacts**. Extract it once and launch the executable. The prebuilt version runs without Python or a compiler.

| Platform | Package | Launch |
| --- | --- | --- |
| Windows x86-64 | `smb3-pc-windows-x86_64.zip` | Extract the entire ZIP into a writable folder and open `smb3.exe`. Keep `SDL2.dll` next to it. |
| Linux x86-64, built on Ubuntu 24.04 | `smb3-pc-linux-x86_64.tar.gz` | Extract the archive and run `./smb3` from the extracted folder. |

On Windows, if the game reports a missing MSVC runtime DLL such as `VCRUNTIME140.dll` or `MSVCP140.dll`, install the x64 redistributable from Microsoft. [Visual C++ Redistributable downloads](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/)

On Ubuntu 24.04, install the runtime libraries and Zenity for the ROM picker. Launch from a desktop session.

```sh
sudo apt install libsdl2-2.0-0 libgl1 libopengl0 libfreetype6 libharfbuzz0b zenity
tar -xzf smb3-pc-linux-x86_64.tar.gz
cd smb3-pc-linux-x86_64
./smb3
```

### First launch

1. Select your uncompressed **Super Mario Bros. 3 (USA), PRG0** `.nes` file when prompted. If your dump is in a ZIP, extract the `.nes` first.
2. Accept the notice to start the theatrical intro. Enter or Start skips the intro.
3. Choose **Play**. Use left and right to pick a character and up and down to select **1 Player** or **2 Players**. Confirm with Enter or A.
4. Move around the world map and enter a stage with Z or A.

The original game has no save, so the remaster saves your campaign each time you reach the world map. Choose **Continue** on the title screen to resume from there after closing the game, or **New game** to start over. **Title screen** in the pause menu keeps the current session; **Continue** returns to it. The save lives in `saves/campaign.cycstate` beside the executable.

The ROM is the only game file you need to supply. The default remastered presentation includes themed backgrounds, layered scenery, animated blocks and coins, translucent water, and input hints that switch between keyboard and Xbox-style controllers. Resource packs add Toad, Toadette, Luigi's own design, and more artwork.

If the file picker is unavailable, pass the ROM path directly, for example `./smb3 "/path/to/game.nes"` on Linux or `.\smb3.exe "C:\Games\game.nes"` in PowerShell. The picker validates the ROM and remembers its path for the next launch. Keep `config.ini`, `rom.cfg`, `saves/`, and `resource_packs/` from your old folder when moving to a new release. Save states are tied to a compatible ROM and engine build.

The ROM checker expects 393,232 bytes including the original iNES header. Use this SHA-256 to identify the exact revision if the picker rejects a file.

```text
6ea0777ca520ba7ad7b0ea0f6452140d59aa0b1dffe5045cb1e8020dde10c267
```

### Resource packs: Toad, Toadette, Luigi, and more art

This project follows the rule of Super Mario Bros. Remastered: the repository and the downloads contain only code and artwork made for the project. Nintendo's artwork comes from your ROM. Artwork made by other fans is installed as resource packs. Download each pack from its author and place it in the `resource_packs` folder beside the executable. The game creates this folder on first launch and reads the packs when it starts.

| Download from the author | Place in `resource_packs` | Adds |
| --- | --- | --- |
| [SMB3-mastered v0.9](https://gamebanana.com/mods/621828) by RadzPrower | Extract the ZIP. It creates `SMB3-mastered/`. | Remastered skies and hills, animated question blocks, bricks and coins, and the Small, Big and Fire sheets for Toad and Toadette |
| [Expanded SMM2 SMB3 sprites, sheet 143355](https://www.spriters-resource.com/custom_edited/mariocustoms/sheet/143355/) by GlacialSiren484, SuperCoolFernie and PixelMarioXP | The PNG as downloaded, 1961 × 4290 | Every suit for Toad and Toadette, and Luigi's own design during levels |
| [Expanded SMM2 SMB3 sprites, updated sheet](https://www.newgrounds.com/art/view/glacialsiren484/sprite-sheet-expanded-smm2-smb3-sprites-for-mario-luigi-toad-and-toadette) by GlacialSiren484 | The WebP as downloaded, 1961 × 4246 | Character-select and map poses, and jump and flight poses |
| [Controller & Keyboard Icons](https://vryell.itch.io/controller-keyboard-icons) by Vryell | Extract the ZIP. It creates `Controllers and Keyboard/`. | Keyboard and Xbox button guides |

Toad and Toadette appear in character selection when the first two packs are installed. The file names can change, because the game recognizes each sheet by its size. Extract ZIP files before starting the game. If a sheet is a different revision, the game keeps the ROM poses for that part. Each pack keeps its author's terms, so every player downloads their own copy. Vryell's terms forbid redistributing the icons.

### Default controls

| Action | Keyboard | Xbox-style controller |
| --- | --- | --- |
| Move and navigate menus | Arrow keys | D-pad or left stick |
| Jump / enter a stage | Z | A |
| Run, fire or open the map inventory | X | X |
| Confirm in menus | Enter | A |
| Pause | Esc or Enter during play | RB or Start |
| Back / resume | Esc or Backspace | B |
| Fullscreen | F11 or Alt+Enter | Configurable |
| Save-state browser / rewind | F7 / F8 | View + left stick click / View + right stick click |
| Take a screenshot | F12 | Configurable |
| Fast-forward while held | Tab | Right trigger |

**Settings → Display** controls the aspect mode and window scale. **Settings → Controls** lets you remap the controls for each player. **Settings → Practice** opens the tools for level selection, powers, and lives.

### Menu language

English is the default. Open **Settings → System → Language** and use Left or Right to switch between **English** and **Español**. The menus update immediately and the game remembers your choice. To switch back from Spanish, use **Configuración → Sistema → Idioma**. Text rendered by the original campaign uses the ROM's language.

## Project status

The ROM supplies the game logic, stages, music, effects, and base graphics. The runtime reads the original tile patterns and palettes when you start the game and combines them with generated background layers and highlights. The grassland scenery reuses the triangular texture from the ROM. Mario and Luigi are available with these default graphics. Resource packs add alternate backgrounds and character artwork and enable Toad and Toadette with the original gameplay rules.

Local testing covers title and character selection, pause, the route into World 1-1, save and restore, and all eight world maps. Longer recorded runs reached later worlds and the final castle, then lost input synchronization before the ending. The test report records the exact coverage and assisted setups. [Validation notes in Spanish](docs/VALIDATION.md)

### What still needs polish

- **Campaign verification.** Complete manual playthroughs through the credits, including alternate routes, bonus rooms, airships, bosses, and the ending. Verify transitions and repeated save, load, and rewind across more of the game.
- **Widescreen and visual consistency.** Review auto-scrolling and vertical rooms, camera changes, enemies at the edge of the viewport, and sprite layering. Several special scenes still use a centered NES-sized composition. More rooms, tile sets, and animations need the same visual pass as the early worlds.
- **Characters and audio.** Check every suit, map animation, and special pose. Some poses fall back to the ROM art when an optional atlas has gaps. Music and effects use the original NES sound engine with band-limited synthesis and a pause filter. Audio balance and playback on real devices still need testing.
- **Platform and input testing.** Test released builds on Windows desktop machines and more Linux setups, with physical controllers, two-player sessions, different refresh rates, and ultrawide displays. macOS and ARM packages still need build and test coverage. Profile demanding levels on lower-powered machines.
- **Accessibility and usability.** Check the menus at smaller window sizes and improve accessibility and error messages based on player feedback.

## Build from source

Use Python 3.11 or newer, Git, CMake 3.20 or newer, and a C11/C++17 compiler. The first build downloads the pinned versions of the dependencies. [Dependency revisions](dependencies.json)

**Ubuntu 24.04**

```sh
sudo apt install build-essential cmake ninja-build python3 git libsdl2-dev libgl-dev zenity
python3 tools/smb3.py build
./build/smb3
```

**Windows x86-64**

Install Visual Studio 2022 or its Build Tools with **Desktop development with C++** and a Windows SDK. Have Git, CMake, and Python available on your PATH. The build uses the SDL2 SDK supplied by NESRecomp.

```powershell
py -3 tools/smb3.py build
.\build\Release\smb3.exe
```

`play.sh` and `play.cmd` provide a source-checkout launcher that builds on first use and accepts a `.nes` or a ZIP containing one ROM. In a terminal, `python3 tools/smb3.py import-rom "/path/to/game.zip"` imports a validated copy to `private/baserom.nes`. Use `py -3` on Windows.

### Resource packs in a source build

Place the resource packs in `resource_packs/` beside the built executable, `build/` on Linux or `build/Release/` on Windows. The source build reads them the same way as a release. Copy them again if you delete the build folder.

## Contribute

Bug reports, playtesting, translations, performance measurements, and pull requests all help. One reproducible issue in an untested level or mode is a useful contribution.

When reporting a bug, include the version or commit, OS, character, world and stage, window size or aspect mode, and steps to reproduce it. A short input sequence or a log can help. Keep your ROM, extracted game data, and save files local.

Before opening a pull request, run the tests from the repository root. Use the appropriate Python command for your platform.

```sh
python3 -m unittest discover -s tests -v
ctest --test-dir build -C Release --output-on-failure
```

For changes to gameplay or rendering, the validation notes list ROM-dependent local checks. The GitHub builds run without a ROM. [Validation notes in Spanish](docs/VALIDATION.md)

## Build artifacts and releases

The workflow compiles and tests the runtime for Windows and Linux on branch pushes, pull requests, and manual runs. The platform archives and the source ZIP are uploaded directly to **Artifacts**, ready to extract. [Build workflow](.github/workflows/check.yml)

To build the packages locally after a standard build:

```sh
cpack --config build/CPackConfig.cmake -C Release
python3 tools/smb3.py source-zip
```

Packages appear in `dist/`. Maintainers can push a version tag to create a draft GitHub release.

```sh
git tag v0.2.0
git push origin v0.2.0
```

After both builds pass, the workflow attaches both platform archives and the source ZIP to the draft. Review and publish it from **Releases** when ready.

## Credits and licenses

- **Nintendo and the original Super Mario Bros. 3 team** created the game, graphics, characters, and audio that the runtime reads from the player's ROM.
- **Matthew Stanley and contributors** built NESRecomp and recomp-ui, which provide the runtime, launcher, and settings. [NESRecomp](https://github.com/mstan/nesrecomp) · [recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui)
- **JHDev2006 and the SMB1 Remastered contributors** inspired the widescreen presentation, ROM import, character selector, and pause experience. [Super Mario Bros. Remastered](https://github.com/JHDev2006/Super-Mario-Bros.-Remastered-Public)
- **Southbird** documented SMB3's routines and memory layout. [SMB3 disassembly](https://github.com/captainsouthbird/smb3)
- **RadzPrower** created the SMB3-mastered resource pack. [SMB3-mastered](https://gamebanana.com/mods/621828)
- **GlacialSiren484, SuperCoolFernie, and PixelMarioXP** created the expanded character sheets used for Luigi, Toad, and Toadette. [Sheet 143355](https://www.spriters-resource.com/custom_edited/mariocustoms/sheet/143355/) · [Updated sheet](https://www.newgrounds.com/art/view/glacialsiren484/sprite-sheet-expanded-smm2-smb3-sprites-for-mario-luigi-toad-and-toadette)
- **Vryell** created the keyboard and controller icons using **PineTreePizza's** Rosy-42 palette. [Controller & Keyboard Icons](https://vryell.itch.io/controller-keyboard-icons)

The integration code and tools use the MIT license. The combined executable includes NESRecomp under PolyForm Noncommercial 1.0.0 with additional permissions. Third-party works retain their own terms. The notices also credit the libraries, reference sheets, and recorded test routes. [Project license](LICENSE) · [Third-party notices](THIRD_PARTY.md)
