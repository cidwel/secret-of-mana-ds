#!/usr/bin/env python3
"""Builds the widescreen Snes9x libretro core (Snes9x 1.63 + ws_snes9x.patch) in a new or empty PREFIX.

usage: build_core.py PREFIX [--plain] [--git SRC] [--no-make] [--android]    needs git, g++, make and network access to github.com unless
--git SRC names a local Snes9x git repository (any clone that has the commit below; e.g. the pristine tree of an earlier build) to clone from.
--no-make stops after applying the patch.  --android builds arm64-v8a instead (NDK r28c, API 30, ndk-build; NDK path from
ANDROID_NDK or ~/Android/Sdk/ndk/28.2.13676358); result PREFIX/snes9x/libretro/libs/arm64-v8a/libretro.so.  --plain skips the
patch (the unmodified reference core used by the comparison tests).  The result is a local tool (Snes9x is
non-commercially licensed); it is not distributed.  Android arm64: see docs/companion-widescreen-core-20261017.md.
Core options (libretro): snes9x_som_widescreen = disabled|17|27|46, snes9x_som_widescreen_fallback = normal|black,
snes9x_som_hud = off|hide_except_ring (hide_in_field is an alias), snes9x_som_hires = average|native.  Debug env: SOM_WS_MARGIN=N (1..128, overrides the option), SOM_WS_FIX=0 keeps
the game's own tilemap, SOM_WS_RAW=1 shows layers that failed validation, SOM_WS_BG3=1 keeps BG3 in the side passes,
SOM_WS_OBJ=0 drops sprites in them.  The 768 px wide render is readable through retro_som_ws_get().
"""
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
COMMIT = '1bcc369e89f08243e0a462882fb1f3e42e51de3a'


def run(*cmd, cwd=None):
    print('+', ' '.join(map(str, cmd)))
    subprocess.run([str(c) for c in cmd], cwd=cwd, check=True)


def main():
    prefix = pathlib.Path(sys.argv[1]).resolve()
    plain = '--plain' in sys.argv
    git_src = sys.argv[sys.argv.index('--git') + 1] if '--git' in sys.argv else 'https://github.com/snes9xgit/snes9x.git'
    if prefix.exists() and any(prefix.iterdir()):
        sys.exit('PREFIX must be empty or new')
    prefix.mkdir(parents=True, exist_ok=True)
    src = prefix / 'snes9x'
    run('git', 'clone', '--no-checkout', git_src, src)
    run('git', 'checkout', COMMIT, cwd=src)
    if not plain:
        run('git', 'apply', HERE / 'ws_snes9x.patch', cwd=src)
    if '--no-make' in sys.argv:
        return
    if '--android' in sys.argv:
        import os
        ndk = pathlib.Path(os.environ.get('ANDROID_NDK', os.path.expanduser('~/Android/Sdk/ndk/28.2.13676358')))
        mk = src / 'libretro/jni/Android.mk'
        # the stock makefile resolves CORE_DIR to a doubled jni/jni path with this NDK; an absolute path avoids it
        mk.write_text(mk.read_text().replace('CORE_DIR := $(LOCAL_PATH)/../..', 'CORE_DIR := %s' % src))
        run(ndk / 'ndk-build', '-j8', 'APP_ABI=arm64-v8a', 'APP_PLATFORM=android-30', cwd=src / 'libretro')
        print('core:', src / 'libretro/libs/arm64-v8a/libretro.so')
        return
    run('make', '-j8', cwd=src / 'libretro')
    print('core:', src / 'libretro/snes9x_libretro.so')


if __name__ == '__main__':
    main()
