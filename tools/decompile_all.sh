#!/usr/bin/env bash
# Decompile the A2 Racer game modules with REA (overview + snapshot) and
# Ghidra headless (full C export), using recovered symbols and Watcom's
# register calling convention. Needs JAVA_HOME and GHIDRA_INSTALL_DIR.
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
[ ${#modules[@]} -eq 0 ] && modules=(A2MENU.DAT A2.EXE A2SPEL.DAT A2MOVIE.DAT)

mkdir -p "$ROOT/build/snapshots" "$ROOT/build/ghidra" "$ROOT/build/logs" "$ROOT/build/names" \
	"$ROOT/symbols/manual"

# Symbols: A2MENU.DAT still carries Watcom debug info; the other modules get
# names carried over from it by byte-pattern and call/reference matching.
python3 -I "$ROOT/tools/watcom_debug.py" "$DISC/A2MENU.DAT" "$ROOT/build/watcom/A2MENU.DAT"
cp "$ROOT/build/watcom/A2MENU.DAT/modules.tsv" "$ROOT/symbols/A2MENU.DAT.modules.tsv"
python3 -I "$ROOT/tools/watcom_names.py" "$ROOT/build/watcom/A2MENU.DAT/globals.tsv" \
	"$ROOT/symbols/A2MENU.DAT.tsv"
for m in A2.EXE A2SPEL.DAT A2MOVIE.DAT; do
	python3 -I "$ROOT/tools/match_functions.py" "$DISC/A2MENU.DAT" "$ROOT/symbols/A2MENU.DAT.tsv" \
		"$DISC/$m" "$ROOT/symbols/$m.tsv"
done

for m in "${modules[@]}"; do
	bin="$DISC/$m"
	out="$ROOT/decompiled/$m"
	names="$ROOT/build/names/$m.tsv"
	mkdir -p "$out"
	# A2MENU.DAT's debug info names every real function; the others are partial.
	coverage=partial
	[ "$m" = A2MENU.DAT ] && coverage=complete
	# Hand-assigned names (symbols/manual/<module>.tsv) override recovered ones.
	python3 -I "$ROOT/tools/merge_names.py" "$names" "$ROOT/symbols/$m.tsv" \
		"$ROOT/symbols/manual/$m.tsv"

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
		-import "$bin" -loader PeLoader -scriptPath "$ROOT/tools" \
		-preScript ApplyWatcom.java pre "$names" \
		-postScript ApplyWatcom.java post "$names" "$coverage" \
		-postScript AutoNameVariables.java \
		-postScript ExportDecompiled.java "$out" "$names" \
		> "$ROOT/build/logs/$m.ghidra.log" 2>&1
	grep -h -E 'ApplyWatcom.java>|AutoNameVariables:|ExportDecompiled:' "$ROOT/build/logs/$m.ghidra.log" | sed 's/^INFO  //'
done
