# National Dex

The launcher's **Hub → National Dex** combines saved Pokédex flags from the games
you select. It does not modify saves or count the Pokémon currently in boxes.
Traded-away Pokémon remain caught if the game's Pokédex records them as caught.

On first opening, check the saves that should count in **Save sources**, and
choose **Done**. Newly discovered saves stay unchecked. **Regions** selects debut
generations: Kanto #001–151, Johto #152–251, Hoenn #252–386, Sinnoh #387–493,
Unova #494–649, Kalos #650–721. All six start enabled, including mythical Pokémon.
Forms, genders and shinies count as one species.

The header always shows **caught / total**, completion percentage, and seen count.
Both counts use only the selected regions; filters change the visible entries but
never the completion denominator. For example, selecting Kanto and Johto gives a
total of 251 species.

The **List / Grid** tabs switch browsing layouts while keeping the highlighted
species. Both have All, Unseen, Seen only, and Caught filters. Unseen entries show
a question mark; seen entries show their name and a black silhouette; caught
entries show their name and full-color menu sprite. The detail screen enlarges
the pixel sprite and shows types, Pokédex text, contributing games under **Caught
in** or **Seen in**, and base stats. No identity or artwork is revealed for unseen
entries. Long details scroll inside their panel.

Tap either view tab or a toolbar control to activate it; tap a species to inspect
it. On a controller, arrows navigate the grid or list. Up from the first row
reaches the view tabs; Left/Right there reaches the other toolbar controls, and
Down returns to the selected species. Confirm on an entry opens its overflowing
details; Right from the last grid column or the list also focuses details.
Up/Down scrolls details, and Left/Back returns to browsing. Dragging scrolls either
panel independently, including in landscape.

Selections are stored in `national-dex.cfg` beside launcher settings; the view is
remembered in `national-dex.cfg.view`. Opening the page, changing selections, or
choosing **Refresh** reads the selected saves again on a background worker. Older
requests cannot overwrite newer selections.
Missing or invalid selected saves stay listed, and totals are marked **Partial**.

## Supported data

- GBA: Ruby/Sapphire, Emerald, FireRed/LeafGreen, with recognized emulator trailers.
- DS: Diamond/Pearl, Platinum, HeartGold/SoulSilver, Black/White, Black 2/White 2.
- 3DS reader: decrypted XY and ORAS `main` saves. Only games discovered by the
  launcher are offered; this does not add XY launching to upstream.
- ROM-hack layouts, save states, original Game Boy saves, and independent save-file
  imports are excluded.

The 3DS locator searches the title's own directory for a unique extracted `main`,
ignoring backup/import/romfs directories. It refuses ambiguous results. The actual
ORAS runtime kit is not part of this repository; locating a real runtime save must
be verified once that kit is supplied. A runtime that stores a packed archive
instead of an extracted `main` will need an adapter, and is reported unavailable.

## Artwork

No Pokémon artwork is downloaded or bundled. The preferred source is the user's
unencrypted ORAS game dump (RomFS `a/0/9/1`, GARC/BCLIM menu icons). The existing DS
icon decoder supplies Gen 5 or Gen 4 icons when available. GBA fallback reads the
ROM's `GFRomHeader` graphics interface (as documented by pret's Emerald and FireRed
projects). A GBA revision without that interface requires a DS/ORAS artwork source.

Decoders use bounded reads and synthetic format tests. Encrypted 3DS dumps and
unsupported image encodings show a placeholder without affecting progress.
The visible sprite bounds are scaled with nearest-neighbor pixels. Decoded sprites
are cached in memory and cleared on refresh. SoulSilver artwork and entry text
have been checked in the Linux app with a user-supplied ROM and a synthetic save;
other real-game artwork sources and the 3DS runtime layout still need validation.

Entry text is decoded locally from supported Platinum, HeartGold/SoulSilver,
Black/White and Black 2/White 2 message archives. Gen 5 is preferred for its larger
catalogue; Gen 4 fills gaps. Types and stats come from that same game's personal
data, with Gen 6 species data as a fallback. Entries outside available ROM text
show an explicit unavailable message. No text or artwork is downloaded or bundled.

Format references:

- [pret Emerald graphics interface](https://github.com/pret/pokeemerald/blob/master/src/rom_header_gf.c)
- [Gen 6 filesystem research](https://projectpokemon.org/home/docs/gen-6/oras-file-system-r32/)
- [pk3DS image formats](https://github.com/kwsch/pk3DS/tree/master/pk3DS.Core/CTR/Images)
- [GodMode9 RomFS structures](https://github.com/d0k3/GodMode9/blob/master/arm9/source/game/romfs.h)

## ROM-free checks

Requires CMake, Ninja, Python 3 and a compiler with C++20 support (GCC 13 works).
No SDL installation, ROMs, Android SDK, real saves, or connected device is needed.

```sh
cmake -S tests/national_dex -B build-national-dex/checks -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-national-dex/checks --parallel 3
ctest --test-dir build-national-dex/checks --output-on-failure
```

The suite exercises all supported save families, the second Gen 4 save slot,
checksum failures, bounded reads, unchanged input bytes, aggregation, filters,
persistence, stale-request rejection, synthetic artwork, grid/list navigation,
view switching, touch targets, and detail scrolling in portrait and landscape
with Pokémon Hub enabled and disabled. The standalone tests share the application's
PKSM-Core target through `cmake/PokemonHub.cmake`.

Local validation includes a full Linux DS/GBA build with Pokémon Hub enabled,
using the supplied US SoulSilver ROM and SDL2. Screenshots exercise the actual
launcher with a synthetic demonstration save, including list/grid switching,
seen silhouettes, selected-region totals, and portrait layout. This does not
validate Android device input/rendering or the external 3DS runtime.

## Android build handoff

The DS/GBA build requires:

1. The user's US SoulSilver ROM (game code `IPGE`) for upstream's generated tables.
2. A prepared Pokémon GBA Widescreen source/dependency folder (`SSR_GBA_DIR`), and
   its mGBA checkout under `GBAW_WORK`, as required by upstream's build.
3. JDK 17; Android SDK platform 34 and build-tools 34.0.0; NDK 27.2.12479018;
   CMake, Ninja, Python 3 with Pillow, curl, git and zip.

Once those inputs are available:

```sh
export ANDROID_SDK="$HOME/android-sdk"
export SSR_GBA_DIR="/path/to/prepared/pokemon-gba-wide"
export GBAW_WORK="/path/to/pokemon-gba-wide-work"
tools/android/build_national_dex.sh "/path/to/soulsilver-us.nds"
```

To also include 3DS, set `SSR_3DS_ANDROID_KIT` to the compatible ORAS project's
`builds/android/embed` folder before running the same command. It must contain
the native libraries, Java source, title metadata and manifest snippet. The kit
is optional; without it, the app includes DS/GBA gameplay and National Dex still
lists all 721 species. The maintainer can use their existing kit, but the real
3DS save location and artwork still require verification as described above.

The wrapper enables `SSR_POKEMON_HUB`, builds arm64-v8a, gives the app the separate
ID `io.github.coolman4128.fulldex.nationaldex` and label **Fulldex National Dex**,
and uses a dedicated signing key. Builds default to three compiler jobs; override
`CMAKE_BUILD_PARALLEL_LEVEL` to change that. The Android updater refuses upstream APK
replacement in this build. Keep the signing key for subsequent installs.

Outputs are `build-android-dist/fulldex-national-dex-arm64.apk`, its `.sha256`, and
source revision/diff records. Native libraries are checked for 16 KB alignment.
The wrapper requires the real SoulSilver ROM and GBA dependencies; it does not
replace them with dummy game code.
