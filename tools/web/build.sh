#!/usr/bin/env bash
# Builds the browser version of PocketPet into docs/play.html.
#
#   tools/web/build.sh /path/to/LovyanGFX
#
# Needs Zig (it brings the WebAssembly C/C++ libraries): either `zig` on the
# PATH, or `pip install ziglang` and ZIG="python3 -m ziglang".
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
LGFX="$(cd "${1:?usage: build.sh /path/to/LovyanGFX}" && pwd)/src"
ZIG="${ZIG:-zig}"
BUILD="${BUILD:-$HERE/build}"
mkdir -p "$BUILD"

FLAGS=(-target wasm32-wasi -O2 -fno-exceptions -I"$LGFX" -D__linux__ -DLGFX_LINUX_FB)

objs=()
for f in LGFXBase lgfx_fonts lgfx_v1 lgfx_v1_panel; do
  obj="$BUILD/$f.o"
  if [ ! -f "$obj" ]; then $ZIG c++ "${FLAGS[@]}" -std=c++17 -w -c "$LGFX/lgfx/v1/$f.cpp" -o "$obj"; fi
  objs+=("$obj")
done

$ZIG c++ "${FLAGS[@]}" -std=c++17 -mexec-model=reactor -I"$HERE/shim" \
  -Wno-date-time -Wno-vla-cxx-extension \
  "$HERE/web_main.cpp" "$ROOT/PocketPet/ui.cpp" "$ROOT/PocketPet/pet.cpp" "${objs[@]}" \
  -s -o "$BUILD/pocketpet.wasm"

python3 - "$HERE/play.html" "$BUILD/pocketpet.wasm" "$ROOT/docs/play.html" <<'PY'
import base64, sys
shell, wasm, out = sys.argv[1:]
page = open(shell).read().replace("__WASM_BASE64__", base64.b64encode(open(wasm, "rb").read()).decode())
open(out, "w").write(page)
print(f"wrote {out} ({len(page) // 1024} KB)")
PY
