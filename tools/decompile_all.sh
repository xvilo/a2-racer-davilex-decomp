#!/usr/bin/env bash
# Decompile the A2 Racer game modules with REA (overview + snapshot) and
# Ghidra headless (full C export). Needs JAVA_HOME and GHIDRA_INSTALL_DIR.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# The disc files stay outside this repo; point A2_DISC elsewhere if needed.
DISC="${A2_DISC:-$ROOT/../A2Racer-dump/disc}"
[ -d "$DISC" ] || { echo "disc directory not found: $DISC (set A2_DISC)" >&2; exit 1; }
DISC="$(cd "$DISC" && pwd)"
: "${JAVA_HOME:?set JAVA_HOME to a JDK 21+}"
: "${GHIDRA_INSTALL_DIR:?set GHIDRA_INSTALL_DIR to a Ghidra 12.1.x install}"
export PATH="$JAVA_HOME/bin:$PATH"
REA="npx -y rea-agents@6.7.0"

modules=("$@")
[ ${#modules[@]} -eq 0 ] && modules=(A2.EXE A2MENU.DAT A2SPEL.DAT A2MOVIE.DAT)

mkdir -p "$ROOT/build/snapshots" "$ROOT/build/ghidra" "$ROOT/build/logs"
for m in "${modules[@]}"; do
	bin="$DISC/$m"
	out="$ROOT/decompiled/$m"
	mkdir -p "$out"
	echo "== $m: REA overview"
	$REA analyze "$bin" --provider ghidra \
		--snapshot "$ROOT/build/snapshots/$m.snapshot" --format json \
		> "$out/overview.json" 2> "$ROOT/build/logs/$m.rea.log"
	# Keep machine-specific paths out of the committed evidence.
	sed -i "s|$DISC/|disc/|g" "$out/overview.json"
	echo "== $m: Ghidra full export"
	rm -rf "$ROOT/build/ghidra/$m.gpr" "$ROOT/build/ghidra/$m.rep"
	# The .DAT modules are PE files; force the PE loader since the extension hides it.
	"$GHIDRA_INSTALL_DIR/support/analyzeHeadless" "$ROOT/build/ghidra" "$m" \
		-import "$bin" -loader PeLoader \
		-scriptPath "$ROOT/tools" -postScript ExportDecompiled.java "$out" \
		> "$ROOT/build/logs/$m.ghidra.log" 2>&1
	grep -h 'ExportDecompiled:' "$ROOT/build/logs/$m.ghidra.log"
done
