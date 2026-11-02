# 3DS games: Dekopon's core built into Fulldex

The 3DS tab plays `.3ds`, `.cci`, `.cxi` and `.cia` ROMs from the roms folder with the core of
[Dekopon](https://github.com/PalindromicBreadLoaf/dekopon) (Azahar for the Switch, GPL-2.0-or-later),
linked into the program like the GBA library. Nothing of it is in this repository: Fulldex builds with
or without it (without it, the Switch build starts `dekopon.nro` for the game, and a PC build lists no 3DS games to play).

`dekopon-embed.patch` (against Dekopon `dfba28f`) adds to Dekopon:

- `DEKOPON_EMBED=ON`: the Switch frontend as a library, `DekoponMain(argc, argv)` in place of `main`, and its
  exception handler named `DekoponCrashHandler`, so that Fulldex's `__libnx_exception_handler` can hand it the
  fast-memory faults (`HorizonExceptionDispatch`).
- `DEKOPON_PC=ON`: `src/citra_pc`, an SDL2 + OpenGL window, keyboard / controller / mouse input and the run loop on top of
  the same core (`DekoponPcPlay(rom, dataFolder)`), and `citra_pc_probe`, a stand-in program whose link line the scripts read.

The libraries are not linked as they are: Dekopon brings its own Teakra, glad and, on the Switch, a copy of Mesa's utility
code (NXVK), which clash with Fulldex's. The scripts copy them with those symbols renamed or made local.

## Linux

    git clone --recursive https://github.com/PalindromicBreadLoaf/dekopon && cd dekopon
    git checkout dfba28f && git apply <fulldex>/tools/dekopon/dekopon-embed.patch
    cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=OFF -DENABLE_QT=OFF -DENABLE_TESTS=OFF \
        -DENABLE_ROOM=OFF -DENABLE_WEB_SERVICE=OFF -DENABLE_SCRIPTING=OFF -DENABLE_CUBEB=OFF -DENABLE_OPENAL=OFF \
        -DENABLE_LIBUSB=OFF -DENABLE_DISCORD_RPC=OFF -DENABLE_SDL2=ON -DUSE_SYSTEM_SDL2=ON -DENABLE_VULKAN=OFF \
        -DENABLE_OPENGL=ON -DDEKOPON_PC=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    ninja -C build/linux citra_pc_probe
    python3 <fulldex>/tools/dekopon/make_pc_libs.py build/linux /some/folder
    cmake -S <fulldex> -B build-pc -DSSR_PC_ALL_GAMES=ON -DSSR_DEKOPON_PC_LIBS_FILE=/some/folder/dekopon_pc_libs.cmake

`citra_pc_probe <rom> <data folder>/` plays a ROM without Fulldex (the same code Fulldex calls).

## Switch

Dekopon's Switch build needs NXVK (Mesa's NVK for the Switch) and devkitPro's switch-freetype, -bzip2, -libpng; follow
[Dekopon's README](https://github.com/PalindromicBreadLoaf/dekopon#readme) for them (a container with devkitPro's image works;
`bwrap` can stand in for Docker), configure with `-DDEKOPON_EMBED=ON -DENABLE_LTO=OFF` (and `-g0` in the release flags),
build `dekopon_embed_probe`, then

    make_switch_libs.py <dekopon build folder> /some/folder [--map /src=<dekopon folder> --map /opt/devkitpro=<devkitPro root of the container>]
    cmake -S <fulldex> -B build-switch-all ... -DSSR_DEKOPON_LIBS_FILE=/some/folder/dekopon_libs.cmake

Dekopon starts before anything of Fulldex's own graphics: the list starts the program again for the chosen game, as it does
for DS and GBA games, so that the GPU is free for NXVK.

## Known limits

- The Linux window is not Dekopon's menu: Escape closes it and the list comes back. Its data (saves, settings) is in a
  `dekopon` folder beside the program (`/switch/dekopon` on the Switch).
- Not tested on a Switch console (only that it links); Eden cannot run NXVK, so it says nothing either way.
- Under Eden (no program restart available) the Switch build runs Dekopon with the list's resources still loaded, which fails.
