"""Scene library: warps to a map (first transition that reaches it) and saves a core state + wide render."""
import json
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wslib  # noqa: E402
from survey import warp  # noqa: E402

SCENES = WORKDIR = wslib.WORK / 'ws_scenes'


def load_scene(c, m, frames=260):
    """Loads (building it on first use) the state of map m after the warp fade-in."""
    SCENES.mkdir(parents=True, exist_ok=True)
    p = SCENES / ('map%d.state' % m)
    if p.exists():
        c.load_state(p.read_bytes())
        return
    survey = json.load(open(wslib.OUT / 'survey.json'))
    base = open(wslib.WORK / 'base_vanilla.state', 'rb').read()
    c.load_state(base)
    c.run(2)
    warp(c, survey[str(m)]['trans'])
    c.run(frames)
    p.write_bytes(c.save_state())


if __name__ == '__main__':
    import numpy as np
    from PIL import Image
    maps = [int(x) for x in sys.argv[2].split(',')]
    c = wslib.WCore(wslib.common.ROM_VANILLA)
    tiles = []
    for m in maps:
        load_scene(c, m)
        c.run(3)
        tiles.append(c.wide())
    sheet = np.concatenate(tiles, axis=0)
    Image.fromarray(sheet).save(sys.argv[1])
