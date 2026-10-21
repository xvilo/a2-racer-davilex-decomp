#!/bin/sh
# Builds the web version into port/target/web: index.html, a2racer.js, a2racer.wasm.
# Needs Emscripten (emcc on the PATH, e.g. via emsdk_env.sh) and the Rust target:
#   rustup target add wasm32-unknown-emscripten
# The link flags are in port/.cargo/config.toml. Serve the folder over HTTP to play:
#   python3 -m http.server -d port/target/web
set -e
cd "$(dirname "$0")/.."
cargo build --release --target wasm32-unknown-emscripten
out=target/web
mkdir -p $out
cp web/index.html target/wasm32-unknown-emscripten/release/a2racer.js \
  target/wasm32-unknown-emscripten/release/a2racer.wasm $out/
echo "built $out"
