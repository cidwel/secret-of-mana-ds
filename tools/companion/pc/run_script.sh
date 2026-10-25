#!/bin/bash
# Runs a headless scripted session and prints the JSON lines compactly (state lines without the item table).
# usage: run_script.sh ROM SCRIPT [extra som-pc options]
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
ROM="$1"; SCRIPT="$2"; shift 2
export SOM_PC_CONFIG="${SOM_PC_CONFIG:-$HOME/.local/state/som-decomp/pc/testcfg}" SOM_PC_STATE="${SOM_PC_STATE:-$HOME/.local/state/som-decomp/pc/teststate}"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
mkdir -p "$SOM_PC_CONFIG" "$SOM_PC_STATE" "$HOME/.local/state/som-decomp/pc/shots"
"$HERE/som-pc" --rom "$ROM" --script "$SCRIPT" "$@" 2>&1 | python3 -c "
import sys, json
for l in sys.stdin:
    l = l.rstrip()
    if l.startswith('{'):
        try: d = json.loads(l)
        except Exception: print(l[:300]); continue
        if 'party' in d:
            print('STATE %s f=%d mode=%s safe=%d modal=%d w=%d hp/mp=%s items=%s q=%s slots=%s focus=%s direct=%s' % (d['tag'], d['frame'], d['mode'], d['safe'], d['modal'], d['frame_w'], [(p['hp'], p['mp']) for p in d['party'] if p['present']], ''.join(str(i[1]) if i[1] else '-' for i in d['items']), d['queue'], d['slots'], d['focus'], d['direct']['err']))
        else: print(l[:300])
    else: print(l[:300])
"
