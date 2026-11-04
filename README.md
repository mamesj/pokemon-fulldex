# NoSplitNX

**The single-screen widescreen Pokémon experience, exclusively crafted for Nintendo Switch: one screen, widescreen, 60 fps at 720p.** No more squinting at two tiny stacked DS screens.

> **Notice:** NoSplitNX is a dedicated Nintendo Switch fork of [mamesj/pokemon-fulldex](https://github.com/mamesj/pokemon-fulldex), based on [ClickingMaster/ssrecomp](https://github.com/ClickingMaster/ssrecomp) and [melonDS](https://melonds.kuribo64.net), streamlined and optimized exclusively for a unified single-screen experience on Nintendo Switch hardware.

## Downloads

| Where you play | Download | Installation Guide |
|---|---|---|
| **Nintendo Switch** (Homebrew) | [nosplitnx-switch.zip](../../releases/latest) | [Switch Guide](#on-the-nintendo-switch) |

**All seven DS games in one program:** Pokémon Black 2, White 2, Black, White, SoulSilver, HeartGold, and Platinum (plus experimental Diamond and Pearl).  
**The GBA classics too:** Pokémon Emerald, FireRed, LeafGreen, and Ultra Violet ([see below](#the-gba-games)).

One program for your Switch: launch it and pick your game from the list. The download contains no copyrighted game ROMs or assets: you bring your own clean ROMs, and NoSplitNX loads what it needs directly from them.

## What is it?

A homebrew program for your Nintendo Switch running the melonDS emulator core (built in: there is nothing else to install), plus a custom engine layer that tracks the game state (which screen, which menu, which battle) and dynamically rearranges the DS display into one proper 16:9 widescreen picture.

It is **not a native PC/console port**; it is the original game presented the way it should look on a modern widescreen display.

## Why not just play it in a standard emulator?

| | Standard melonDS | NoSplitNX (Nintendo Switch) |
|---|---|---|
| **Screens** | Two 256×192 DS screens, stacked or side by side with large borders | **One unified 16:9 widescreen display**, the touch screen's controls tucked beside the action |
| **3D World** | 4:3 at original DS resolution | **16:9 widescreen**, skies and scenery carried all the way to the edges, hardware-accelerated by the Switch GPU (deko3D) |
| **Frame rate** | The game's native 30 fps 3D | **60 fps**, with smooth in-between frame interpolation |
| **Performance** | Generic emulation overhead | **Switch-optimised**: smart 3D pass skipping in 2D menus, zero-copy buffer handling, and automatic handheld 720p scaling |

---

## On the Nintendo Switch

![The Switch game list: the DS games, the GBA games and Omega Ruby (COMING SOON) under their headings, with tabs for each console](docs/screenshots/game-list.gif)

### Quick Setup

1. **Download [`nosplitnx-switch.zip`](../../releases/latest)** and extract it to the root of your SD card. It creates `/switch/nosplitnx/` with an empty `roms` folder inside.
2. **Copy your ROMs** into `/switch/nosplitnx/roms/` (clean, unpatched .nds and .gba files; any filenames).
3. On your Switch (running Atmosphere CFW), **hold R while launching any installed game** to open the Homebrew Menu in Title Override mode with full RAM access (launching from the Album applet mode lacks memory and is not supported).
4. Select **NoSplitNX** and choose your game.

In the game list, the D-pad or left stick chooses a game, **A** plays, **Y** opens settings, and **+** exits back to the Homebrew Menu. It remembers and opens on the game you played last.

### Controls

| Button | In Game | In Menu / Game List |
|---|---|---|
| **D-pad / Left Stick** | Move character | Navigate games / settings |
| **A / B** | DS A / B buttons | Confirm (**A**) / Back (**B**) |
| **X / Y** | DS X / Y buttons | **Y**: Open Settings |
| **L / R** | DS L / R shoulder buttons | Switch console tabs (ALL, DS, GBA) |
| **ZR (Hold)** | **Fast Forward** | - |
| **+ (Plus)** | Open Pause Menu | Exit to Homebrew Menu |
| **- (Minus)** | DS SELECT button | - |
| **Touchscreen** | Tap directly (Handheld mode) | - |
| **Right Stick** | Move touch cursor (Docked / Controller) | - |
| **ZL** | Touch / Click cursor at current position | - |
| **R3 (Click Right Stick)**| Swap / show secondary bottom screen | - |

*(You can remap any button under **CONTROLS > KEYS AND BUTTONS** in the settings or pause menu).*

### sys-clk (Recommended Overclock)

For a solid 60 fps in all 3D towns and battles, configuring [`sys-clk`](https://github.com/retronx-team/sys-clk) is recommended:

| Hardware | Handheld Profile | Docked Profile |
|---|---|---|
| **CPU** | 1581 MHz (or 1785 MHz for maximum headroom) | 1785 MHz |
| **GPU** | 460 MHz (official handheld maximum) | 768 MHz |
| **Memory (RAM)** | 1600 MHz | 1600 MHz |

Without sys-clk, the console runs at standard stock clocks and may drop frames in complex 3D scenes.

### HOME Menu Icons (Forwarders)

Start each game directly from your Switch HOME Menu with full box art, without needing a PC or keys:
1. In the NoSplitNX game list, press **Y** to enter **SETTINGS**.
2. Select **HOME MENU ICONS**.
3. It generates forwarders directly on your console for every game found in your `roms/` folder (requires standard sigpatches).

---

## Use your own save

Carry on from your existing save: from another emulator, flash cart, or cartridge dump (Checkpoint, JKSM, GodMode9). Copy your `.sav` into `/switch/nosplitnx/`:

| Game | Save file name |
|---|---|
| Black 2 / White 2 | `black2.sav` / `white2.sav` |
| Black / White | `black.sav` / `white.sav` |
| SoulSilver / HeartGold | `soulsilver.sav` / `heartgold.sav` (Spanish: `heartgold-es.sav`, Italian: `heartgold-it.sav`) |
| Platinum | `platinum.sav` (Spanish: `platinum-es.sav`, French: `platinum-fr.sav`) |
| Emerald / FireRed / LeafGreen | `emerald.sav` / `firered.sav` / `leafgreen.sav` |

- **Save formats:** Standard `.sav` files work directly. If you have a DraStic or DeSmuME `.dsv`, or an Action Replay `.duc`, use [`tools/bw2/convert_save.py`](tools/bw2/convert_save.py).
- **Automated backups:** A `backups` folder beside your saves keeps dated copies automatically (created at startup and every 30 minutes of play), with one-click restore from the pause menu.

---

## Regional support - beta

**Regional DS support is integrated across 42 exact ROM versions.** Bring your own clean dump; supported language profiles validate their fingerprint before loading.

| Language | Games with added profiles |
|---|---|
| French, German, Italian, Spanish, Japanese, Korean | Black, White, Black 2, White 2, SoulSilver, HeartGold, Platinum |
| Experimental | Diamond: English, Spanish, German, Italian. Pearl: English, French, Spanish, German, Korean |

Regional games use distinct save names (e.g. `heartgold-es.sav`, `white-es.sav`).  
**DISPLAY > LANGUAGE** in Settings allows selecting AUTO, English, Spanish, Italian, French, German, Japanese, or Korean menus independently of the ROM language.

---

## Cheats

Action Replay codes, turned on and off directly as you play via **CHEATS** in the pause menu:
- **Built-in Popular Cheats:** Every DS game includes 15 built-in codes (Walk through walls, Max money, Rare Candies, Master Balls, Wild shinies, Always catch, Infinite HP/PP, Double EXP, No encounters, Fast hatching, All badges/TMs, Full Pokédex).
- **Cheat Database:** Supports `usrcheat.dat` (e.g. DeadSkullzJr's database). Place `usrcheat.dat` into `/switch/nosplitnx/cheats/`.
- **Custom Codes:** Create a text file named after the game code (e.g. `IREO.txt` for Black 2, `IPGE.txt` for SoulSilver) in `/switch/nosplitnx/cheats/`.

---

## RetroAchievements

Earn [RetroAchievements](https://retroachievements.org) on your Switch:
1. Open the pause menu (**+**) or Settings.
2. Select **RETROACHIEVEMENTS**, toggle it ON, and log in with your account.
3. Unlocks pop up as you play whenever Wi-Fi is connected. Save states and fast-forward remain enabled in casual mode.

---

## The games

### Pokémon Black 2 and White 2

| | |
|---|---|
| ![Black 2: Black Kyurem against a wild Crustle, with the game's FIGHT, BAG, RUN and POKÉMON panel beside the battle](docs/screenshots/black2-black-kyurem-battle.jpg) | ![White 2: White Kyurem against a wild Tropius, with the FIGHT panel beside the battle](docs/screenshots/white2-white-kyurem-battle.jpg) |
| ![Black 2: the view from Aspertia's lookout, sky and mountains carried to the edges](docs/screenshots/black2-aspertia-lookout.jpg) | ![Black 2: Castelia City's pier](docs/screenshots/black2-castelia-pier.jpg) |

- **No version exclusives:** Turn on `OTHER VERSION POKEMON` in the pause menu to find version-exclusive wild encounters on either version.
- **What you get:** Single, double, triple, and rotation battles with HP boxes at the widescreen borders and touch buttons beside them; all bag, party, and menu screens laid out for one screen; wide map loading loaded around you so scenery never pops out.

### Pokémon SoulSilver and HeartGold

![SoulSilver: a wild Meowth appears and Lugia is sent out, in widescreen](docs/screenshots/soulsilver-lugia-battle.gif)

| | |
|---|---|
| ![HeartGold: Totodile against a wild Pidgey on Route 29, with FIGHT, BAG, RUN, POKÉMON and the party beside the battle](docs/screenshots/heartgold-totodile-battle.jpg) | ![SoulSilver: Lugia following you along Route 7](docs/screenshots/soulsilver-lugia-following.jpg) |

- Johto and Kanto in seamless widescreen, with your partner Pokémon walking behind you.
- Single-screen bag, Pokédex, and party layouts, with battles featuring quick touchscreen buttons beside the combatants.

### Pokémon Black and White

| | |
|---|---|
| ![Black: Reshiram against a wild Mantyke, with the game's FIGHT, BAG, RUN and POKÉMON panel beside the battle](docs/screenshots/black-reshiram-battle.jpg) | ![White: Zekrom in battle, with the FIGHT panel beside it](docs/screenshots/white-zekrom-battle.jpg) |
| ![Black: a double battle, Reshiram and Excadrill against a wild Pelipper and Mantyke, the HP boxes down the edges](docs/screenshots/black-reshiram-double.jpg) | ![Black: Nimbasa City in widescreen](docs/screenshots/black-nimbasa.jpg) |

- Unova's first adventure adapted with Black 2's widescreen engine: battles, menus, Bag, and Pokédex adapted for a single 16:9 display.

### Pokémon Platinum

| | |
|---|---|
| ![Platinum: Infernape against a wild Machoke, with FIGHT, BAG, RUN and POKÉMON beside the battle](docs/screenshots/platinum-infernape-battle.jpg) | ![Platinum: Snowpoint City in the snow, in widescreen](docs/screenshots/platinum-snowpoint.jpg) |

- Sinnoh in widescreen: snow and rain reaching the borders, battle commands at the edge, and quick screen swap for Pokétch apps.

### Pokémon Diamond and Pearl

| | |
|---|---|
| ![Diamond: Giratina against Elite Four Aaron's Dustox, with its moves beside the battle](docs/screenshots/diamond-giratina-battle.jpg) | ![Diamond: the Pokémon League, in widescreen](docs/screenshots/diamond-pokemon-league.jpg) |

- Platinum's widescreen layouts adapted to Diamond & Pearl. Experimental support for English, Spanish, French, German, Italian, and Korean versions.

### The GBA games

![Emerald: a wild Rayquaza appears at the top of Sky Pillar and Salamence is sent out, in widescreen](docs/screenshots/gba-emerald-rayquaza-battle.gif)

- **Pokémon Emerald, FireRed, LeafGreen, and Ultra Violet** running on an embedded widescreen mGBA core.
- Wide overworld viewpoints, extended combat backgrounds, weather extending across the screen, and unified DS controller shortcut mappings.

---

## Under the hood

- **Hardware Acceleration:** The 3D world is drawn directly by the Switch Tegra X1 GPU using Nintendo Switch deko3D.
- **Fast Execution:** melonDS FAST JIT runs ARM9 instructions with minimal overhead.
- **NoSplitNX Optimisations:**
  - Automatically skips invisible 3D rendering passes during 2D menus or secondary screen views (`rc_wide_render_skip`).
  - Zero-copy buffer assignment: eliminates redundant CPU framebuffer allocations when wide geometry is idle.
  - Portable handheld mode detection clamping resolution to 720p native for optimal battery life and framerate stability.

---

## Credits and licence

NoSplitNX is a dedicated Nintendo Switch fork based on:
- [mamesj/pokemon-fulldex](https://github.com/mamesj/pokemon-fulldex) for multi-game unification, regional mappings, and layouts.
- [ClickingMaster/ssrecomp](https://github.com/ClickingMaster/ssrecomp) for the original single-screen static recompiler concept.
- [melonDS](https://melonds.kuribo64.net) for the Nintendo DS emulation core and JIT.
- [mGBA](https://mgba.io) and Pokémon GBA Widescreen for Game Boy Advance emulation.
- [sphaira](https://github.com/ITotalJustice/sphaira) for Nintendo Switch forwarder generation.
- DeadSkullzJr and libretro-database for cheat code databases.

Licensed under **GPL-3.0-or-later** (see [LICENSE](LICENSE)).  
*Not affiliated with or endorsed by Nintendo, Game Freak, or The Pokémon Company. Pokémon is a registered trademark of its respective owners.*

---

Made with ♥ by [IamCarron](https://github.com/IamCarron)

Built with the help of AI.
