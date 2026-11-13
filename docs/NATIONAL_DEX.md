# National Dex

The launcher's **Hub → National Dex** combines saved Pokédex flags from the games
you select. It does not modify saves or count the Pokémon currently in boxes.
Traded-away Pokémon remain caught if the game's Pokédex records them as caught.

On first opening, choose **Select Saves**, check the games that should count, and
choose **Done**. Newly discovered saves stay unchecked. **Regions** selects debut
generations: Kanto #001–151, Johto #152–251, Hoenn #252–386, Sinnoh #387–493,
Unova #494–649, Kalos #650–721. All six start enabled, including mythical Pokémon.
Forms, genders and shinies count as one species.

The list has All, Unseen, Seen but Uncaught, and Caught filters. Unseen entries show
only their number and `???`; seen entries show their name and a silhouette; caught
entries show their name and full-color menu sprite. The detail pane identifies the
contributing saves. A missing artwork source does not change progress or reveal an
unseen Pokémon. On a controller, use Right to enter an overflowing detail pane,
Up/Down to scroll, and Left/Back to return to the species list.

Selections are stored in `national-dex.cfg` beside launcher settings. Opening the
page, changing selections, or choosing **Refresh** reads the selected saves again
on a background worker. Older requests cannot overwrite newer selections.
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
unsupported image encodings produce an artwork-unavailable message. Real game
artwork and the 3DS runtime layout remain unverified until the user's inputs are
available. Decoded sprites are cached in memory and cleared on refresh.

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
persistence, stale-request rejection, synthetic artwork, and launcher navigation
with Pokémon Hub enabled and disabled. The standalone tests share the application's
PKSM-Core target through `cmake/PokemonHub.cmake`.

Local validation also compiled the full native launcher with Pokémon Hub enabled
using upstream's synthetic ROM fixture and a local SDL build. Existing About and
ROM action navigation regressions passed (the optional external-binary About
case was skipped). This verifies compilation and synthetic behavior, not real
game-file compatibility or Android packaging.

## Android build handoff

The full build requires:

1. The user's US SoulSilver ROM (game code `IPGE`) for upstream's generated tables.
2. A prepared Pokémon GBA Widescreen source/dependency folder (`SSR_GBA_DIR`), and
   its mGBA checkout under `GBAW_WORK`, as required by upstream's build.
3. The compatible ORAS Android embed kit (`SSR_3DS_ANDROID_KIT`), including its
   native libraries, Java source, title metadata and manifest snippet.
4. JDK 17; Android SDK platform 34 and build-tools 34.0.0; NDK 27.2.12479018;
   CMake, Ninja, Python 3 with Pillow, curl, git and zip.

Once those inputs are available:

```sh
export ANDROID_SDK="$HOME/android-sdk"
export SSR_GBA_DIR="/path/to/prepared/pokemon-gba-wide"
export GBAW_WORK="/path/to/pokemon-gba-wide-work"
export SSR_3DS_ANDROID_KIT="/path/to/3ds/builds/android/embed"
tools/android/build_national_dex.sh "/path/to/soulsilver-us.nds"
```

The wrapper enables `SSR_POKEMON_HUB`, builds arm64-v8a, gives the app the separate
ID `io.github.coolman4128.fulldex.nationaldex` and label **Fulldex National Dex**,
and uses a dedicated signing key. The Android updater refuses upstream APK
replacement in this build. Keep the signing key for subsequent installs.

Outputs are `build-android-dist/fulldex-national-dex-arm64.apk`, its `.sha256`, and
source revision/diff records. Native libraries are checked for 16 KB alignment.
The wrapper requires the full dependencies; it does not replace them with dummy
game code. No Android APK has been produced without those inputs.
