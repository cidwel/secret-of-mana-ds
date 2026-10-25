#!/bin/bash
# Runs scripts/s4_generic.txt on one ROM with its party state.  usage: test_roms.sh LABEL ROM STATE
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
LABEL="$1"; ROM="$2"; ST="$3"
D="$HOME/.local/state/som-decomp/pc"
T="$D/s4_$LABEL.txt"
{ echo "LOAD $ST"; sed "s#@SHOTS@#$D/shots/s4_$LABEL#g" "$HERE/scripts/s4_generic.txt"; } > "$T"
SOM_PC_CONFIG="$D/testcfg_$LABEL" "$HERE/run_script.sh" "$ROM" "$T"
