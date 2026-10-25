#!/usr/bin/env python3
"""CPU-time frame rate of the plain core versus the widescreen core (option off / 17 / 27 / 46, HUD mask).

usage: core_bench.py OUT.json [FRAMES] [ROUNDS]   (field walk in Potos, video callback does not copy the frame)
Uses process CPU time, so the figure is not distorted by other load on the machine."""
import json
import sys
import time
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402


class Fast(wscore.WS):
    def _vid(self, d, w, h, pitch):
        self.width, self.height = w, h


def bench(core, opts, frames, state):
    c = Fast(wscore.common.ROM_VANILLA, core, opts)
    c.load_state(state)
    c.run(30)
    pads = [c.mask(x) for x in ('RIGHT', 'DOWN', 'LEFT', 'UP')]
    t0 = time.process_time()
    for f in range(frames):
        c.set_pad(pads[(f // 60) % 4])
        c.step()
    return frames / (time.process_time() - t0)


def main():
    out = sys.argv[1]
    frames = int(sys.argv[2]) if len(sys.argv) > 2 else 1500
    rounds = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    state = open(wscore.WORK / 'party3_vanilla.state', 'rb').read()
    cases = [('plain', wscore.CORE_PLAIN, {}),
             ('ws core, option off', wscore.CORE_WS, {}),
             ('17 px', wscore.CORE_WS, {'snes9x_som_widescreen': '17'}),
             ('27 px', wscore.CORE_WS, {'snes9x_som_widescreen': '27'}),
             ('46 px', wscore.CORE_WS, {'snes9x_som_widescreen': '46'}),
             ('27 px + HUD hide', wscore.CORE_WS, {'snes9x_som_widescreen': '27', 'snes9x_som_hud': 'hide_in_field'}),
             ('HUD hide only', wscore.CORE_WS, {'snes9x_som_hud': 'hide_in_field'})]
    res = {n: [] for n, _, _ in cases}
    for r in range(rounds):
        for n, core, opts in cases:
            res[n].append(round(bench(core, opts, frames, state), 1))
            print(r, n, res[n][-1], flush=True)
    summ = {n: max(v) for n, v in res.items()}
    json.dump({'fps_cpu_best': summ, 'all': res, 'frames': frames}, open(out, 'w'), indent=1)
    base = summ['plain']
    for n, v in summ.items():
        print('%-22s %8.1f fps  %5.1f %% of plain' % (n, v, 100 * v / base))


main()
