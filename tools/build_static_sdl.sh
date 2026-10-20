#!/usr/bin/env bash
# Builds SDL2 and SDL2_ttf (with its own FreeType, no HarfBuzz) as static libraries
# into $1 (default ./deps), for self-contained release builds of the port. Extra
# arguments go to every CMake configure (e.g. the macOS architectures).
# Used by .github/workflows/build.yml.
set -euo pipefail
SDL2_VERSION=${SDL2_VERSION:-2.30.9}
SDL2_TTF_VERSION=${SDL2_TTF_VERSION:-2.22.0}
prefix=$(mkdir -p "${1:-deps}" && cd "${1:-deps}" && pwd)
shift || true

# Each step's output goes to a log; on failure its errors become GitHub annotations,
# which can be read without logging in.
run() {
	log=$1
	shift
	if ! "$@" > "$log" 2>&1; then
		# At most 10 annotations show per step: the errors first.
		{ grep -iE -A4 "CMake Error|error:" "$log" || tail -10 "$log"; } | grep -v '^--$' | head -10 | tr -d '\r' | sed 's/^/::error::/'
		tail -60 "$log"
		exit 1
	fi
}

run dl1.log curl -sSfL -o sdl.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VERSION/SDL2-$SDL2_VERSION.tar.gz
run dl2.log curl -sSfL -o ttf.tar.gz https://github.com/libsdl-org/SDL_ttf/releases/download/release-$SDL2_TTF_VERSION/SDL2_ttf-$SDL2_TTF_VERSION.tar.gz
tar xzf sdl.tar.gz
tar xzf ttf.tar.gz
# CMake 4 refuses the vendored FreeType's old cmake_minimum_required without the policy minimum.
common=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_PREFIX_PATH="$prefix"
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "$@")
run sdl-cfg.log cmake -S SDL2-$SDL2_VERSION -B sdl-build "${common[@]}" -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF
run sdl-build.log cmake --build sdl-build -j 4
run sdl-install.log cmake --install sdl-build
run ttf-cfg.log cmake -S SDL2_ttf-$SDL2_TTF_VERSION -B ttf-build "${common[@]}" -DBUILD_SHARED_LIBS=OFF -DSDL2TTF_VENDORED=ON -DSDL2TTF_HARFBUZZ=OFF -DSDL2TTF_SAMPLES=OFF
run ttf-build.log cmake --build ttf-build -j 4
run ttf-install.log cmake --install ttf-build
