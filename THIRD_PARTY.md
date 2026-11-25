# Third-party components and references

| Component | Author / project | Revision / version | Terms |
| --- | --- | --- | --- |
| [NESRecomp](https://github.com/mstan/nesrecomp) | Matthew Stanley and contributors | `77ad5b8264d7b60438aec1e1466df710c890f319` | PolyForm Noncommercial 1.0.0 + additional permissions |
| [recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui) | Matthew Stanley and contributors | `7c9eea3e5c4050223cd91c42ba8c076a99ae79c9` | MIT |
| [recomp-net](https://github.com/RetroPortingToolKit/recomp-net) | recomp-net contributors | `265945868b1dce4ff8aaffc24df7e367dd0b32cd` | MIT |
| [rbengine](https://github.com/RetroPortingToolKit/rbengine) | retcomm-rbengine contributors | `2a03e73693acee0fb78076ea058642c931bef12e` | MIT |
| [SDL2](https://github.com/libsdl-org/SDL/tree/release-2.30.0) | Sam Lantinga and SDL contributors | 2.30.0 SDK on Windows, system SDL2 on Linux | zlib |
| Dear ImGui | Omar Cornut and contributors | Vendored by pinned recomp-ui | MIT |
| emu2413 | Mitsutaka Okazaki and contributors | Vendored by pinned NESRecomp | MIT |
| LatoLatin | Lukasz Dziedzic | Lato 2.015 Latin subset | SIL OFL 1.1 |
| Noto Sans Symbols 2; flag images from Noto Color Emoji | The Noto Project Authors | Vendored by pinned recomp-ui | SIL OFL 1.1 |
| OpenMoji | OpenMoji Project | Vendored by pinned recomp-ui | CC BY-SA 4.0 |

License texts and asset notices are retained under `licenses/`. A combined executable includes NESRecomp code and is subject to that component's terms; this repository's MIT license does not replace them. The network libraries are linked because the shared menu requires them; online play is not integrated for SMB3 here.

The launcher also uses `stb_image`, `stb_image_write` (Sean Barrett and contributors; public domain / MIT alternative) and `tinyfiledialogs` (Guillaume Vareille; zlib), vendored in recomp-ui with their notices in the source files. FreeType and HarfBuzz are optional system components; preserve their system license notices if bundling their binaries.

## References

- [Super Mario Bros. Remastered](https://github.com/JHDev2006/Super-Mario-Bros.-Remastered-Public), JHDev2006 and contributors: requested presentation reference; Godot project, GPL-3.0. Studied revision `7fe1ff7340ec7fd8ace6854faa0e620dd7dba05a`, in particular `RomAssetRipper.gd` and the sprite metadata. Also inspected camera, window, enemy notifier, despawner, generator and projectile scripts in the user's local Godot 4.6.3 PCK. Also inspected its title options, character selection, disclaimer and settings flow. Their viewport-based activation, fullscreen behavior and game-style menus informed the SMB3 adaptation. No GPL source, templates or artwork from that project is bundled in this integration.
- **SMB3-mastered v0.9** by RadzPrower. The optional importer reads the player's local ZIP. The author's download page credits the source material and lists the pack's CC BY-NC-ND 4.0 terms. [SMB3-mastered](https://gamebanana.com/mods/621828)
- **Expanded SMM2 SMB3 Sprites for Mario, Luigi, Toad and Toadette** by GlacialSiren484, SuperCoolFernie and PixelMarioXP. The optional importer prepares map, airborne, front and victory poses from a local copy. [Expanded SMB3 sprites](https://mfgg.net/index.php?act=resdb&param=02&c=1&id=41350)
- **Controller & Keyboard Icons** by Vryell, using **Rosy-42** by PineTreePizza. The optional importer prepares keyboard and Xbox hints from a local copy. The author provides the pack and its terms. [Controller & Keyboard Icons](https://vryell.itch.io/controller-keyboard-icons)
- **Nintendo** created the Super Mario Maker 2 artwork used as visual reference. **Random Talking Bush** ripped the sheets hosted by The Spriters Resource. [Characters and items](https://www.spriters-resource.com/nintendo_switch/supermariomaker2/asset/119042/) · [Enemies](https://www.spriters-resource.com/nintendo_switch/supermariomaker2/asset/119432/) · [Tileset](https://www.spriters-resource.com/nintendo_switch/supermariomaker2/asset/119280/) · [Backgrounds](https://www.spriters-resource.com/nintendo_switch/supermariomaker2/asset/119279/)
- **DogToon64** ripped Nintendo's Super Mario Maker 2 clear condition icons used as a presentation reference. [Clear Condition Icons (SMB3)](https://www.spriters-resource.com/nintendo_switch/supermariomaker2/asset/167207/)
- [Super Mario Bros. 3 disassembly](https://github.com/captainsouthbird/smb3), Southbird. [Original project](http://www.sonicepoch.com/sm3mix/). Used to interpret RAM locations and game behavior. The disassembly and its CHR assets are not bundled in the public sources.
- [SMB3 all-levels tool-assisted speedrun, TASVideos #2835](https://tasvideos.org/2835M), Lord_Tom and Tompa. Used locally to exercise gameplay. The original recording is not included in the source package.
- [SMB3 warps tool-assisted speedrun, TASVideos #3922](https://tasvideos.org/3922M), Lord_Tom, Maru and Tompa. Used locally to exercise the World 8 route. The recording is not included in the source package.

The original game, its graphics, music and characters are Nintendo material. Game sprites, map and level data, the menu logo, cursor and letter bitmaps are read from the player's local ROM. Additional artwork stays in the local import and retains the ownership and credits described above. MIT covers the integration code.
