#!/usr/bin/env bash
# Full, side-by-side National Dex build; never substitutes synthetic game code.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$HERE"
if [ $# -ne 1 ]; then
    echo 'Usage: tools/android/build_national_dex.sh /path/to/soulsilver-us.nds' >&2
    exit 2
fi
: "${SSR_GBA_DIR:?Set SSR_GBA_DIR to the GBA build dependency}"
: "${SSR_3DS_ANDROID_KIT:?Set SSR_3DS_ANDROID_KIT to the compatible 3DS Android embed kit}"
export SSR_POKEMON_HUB=ON
export SSR_ANDROID_ABIS=arm64-v8a
export SSR_ANDROID_PACKAGE=io.github.coolman4128.fulldex.nationaldex
export SSR_ANDROID_KEY="${SSR_ANDROID_KEY:-$HOME/.config/pokemon-ds-wide/national-dex-signing-key.json}"
mkdir -p build-national-dex
python3 - <<'PY'
from pathlib import Path
source = Path('tools/android/allgames/AndroidManifest.xml').read_text()
source = source.replace('android:label="Pokémon Fulldex"', 'android:label="Fulldex National Dex"')
Path('build-national-dex/AndroidManifest.xml').write_text(source)
PY
export SSR_ANDROID_MANIFEST="$HERE/build-national-dex/AndroidManifest.xml"
bash tools/android/build_all.sh "$1"
python3 tools/android/check_native_alignment.py build-android-dist/pokemon-ds-wide-android.apk
cp build-android-dist/pokemon-ds-wide-android.apk build-android-dist/fulldex-national-dex-arm64.apk
sha256sum build-android-dist/fulldex-national-dex-arm64.apk > build-android-dist/fulldex-national-dex-arm64.apk.sha256
git rev-parse HEAD > build-android-dist/national-dex-source-commit.txt
git diff --binary HEAD > build-android-dist/national-dex-source.patch
