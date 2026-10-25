#!/usr/bin/env python3
"""Build Secret of Mana DS from source: the Thor (Android) APK, the Nintendo 3DS .3dsx and the PC version.

usage: python3 build.py {thor,3ds,pc,all} [--docker] [--no-rando]

No ROM is needed: every app reads your ROM when it runs.  Outputs go to dist/.

Third-party sources are fetched at pinned commits into .deps/ (needs git and network once):
  libretro/snes9x          Android emulator core
  snes9xgit/snes9x         widescreen core (with tools/companion/widescreen/ws_snes9x.patch)
  libretro/snes9x2005      3DS emulator core
  Moppu/SecretOfManaRandomizer   build-time resources of the randomizer (Ancient Cave, open world...)

Toolchains, found through environment variables:
  thor  JDK 17 (JAVA_HOME or java on PATH), Android SDK (ANDROID_HOME or ANDROID_SDK_ROOT) with NDK 28.2.13676358 and CMake 3.22.1
  3ds   devkitPro with devkitARM and libctru (DEVKITPRO), make
  pc    gcc, make, pkg-config and SDL2 (Linux) or MSYS2 MINGW64 (Windows)
--docker runs the build inside the image of docker/Dockerfile instead, so only Docker is needed (any OS).
"""
import argparse
import concurrent.futures
import os
import pathlib
import platform
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent
COMPANION = ROOT / 'tools/companion'
DEPS = ROOT / '.deps'
BUILD = ROOT / '.build'
DIST = ROOT / 'dist'
WINDOWS = platform.system() == 'Windows'
NDK_VERSION = '28.2.13676358'
IMAGE = 'secret-of-mana-ds-build'

SOURCES = {
    'snes9x-libretro': ('https://github.com/libretro/snes9x', 'fae2fea08f74180759ef540ee94259213f503480'),
    'snes9x-ws': ('https://github.com/snes9xgit/snes9x', '1bcc369e89f08243e0a462882fb1f3e42e51de3a'),
    'snes9x2005': ('https://github.com/libretro/snes9x2005', 'a79dfe9047e7fec58808aefe48ad2bf499c7af11'),
    'randomizer': ('https://github.com/Moppu/SecretOfManaRandomizer', '1404b7bd8552677399f24632c1e37cba37d491ef'),
}


def run(cmd, cwd=None, env=None):
    print('+', ' '.join(map(str, cmd)), flush=True)
    subprocess.run([str(c) for c in cmd], cwd=cwd, env=env, check=True)


def fetch(name):
    """A clean checkout of a pinned source; re-fetched when the commit is not there."""
    url, commit = SOURCES[name]
    dest = DEPS / name
    if (dest / '.git').is_dir():
        head = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=dest, capture_output=True, text=True).stdout.strip()
        if head == commit:
            run(['git', 'checkout', '-q', '--', '.'], cwd=dest)
            run(['git', 'clean', '-qfdx'], cwd=dest)
            return dest
        shutil.rmtree(dest)
    dest.mkdir(parents=True)
    run(['git', 'init', '-q'], cwd=dest)
    # keep upstream LF endings on Windows, or the widescreen patch does not apply
    run(['git', 'config', 'core.autocrlf', 'false'], cwd=dest)
    run(['git', 'fetch', '-q', '--depth', '1', url, commit], cwd=dest)
    run(['git', 'checkout', '-q', 'FETCH_HEAD'], cwd=dest)
    return dest


def need(var, hint):
    value = os.environ.get(var)
    if not value or not pathlib.Path(value).is_dir():
        sys.exit('%s is not set or not a folder: %s' % (var, hint))
    return pathlib.Path(value)


def ndk_build(src):
    """Builds the arm64-v8a libretro core of a snes9x tree with ndk-build."""
    sdk = pathlib.Path(os.environ.get('ANDROID_HOME') or os.environ.get('ANDROID_SDK_ROOT') or '')
    ndk = pathlib.Path(os.environ.get('ANDROID_NDK_HOME') or sdk / 'ndk' / NDK_VERSION)
    if not ndk.is_dir():
        sys.exit('Android NDK %s not found: install it with sdkmanager "ndk;%s" or set ANDROID_NDK_HOME' % (NDK_VERSION, NDK_VERSION))
    mk = src / 'libretro/jni/Android.mk'
    # the stock makefile resolves CORE_DIR to a doubled jni/jni path with this NDK; an absolute path avoids it
    mk.write_text(mk.read_text().replace('CORE_DIR := $(LOCAL_PATH)/../..', 'CORE_DIR := %s' % src.as_posix()))
    tool = ndk / ('ndk-build.cmd' if WINDOWS else 'ndk-build')
    run([tool, '-j%d' % (os.cpu_count() or 4), 'APP_ABI=arm64-v8a', 'APP_PLATFORM=android-30'], cwd=src / 'libretro')
    return src / 'libretro/libs/arm64-v8a/libretro.so'


def build_thor(rando):
    plain = ndk_build(fetch('snes9x-libretro'))
    ws_src = fetch('snes9x-ws')
    run(['git', 'apply', COMPANION / 'widescreen/ws_snes9x.patch'], cwd=ws_src)
    ws = ndk_build(ws_src)
    project = COMPANION / 'android'
    gradle = project / ('gradlew.bat' if WINDOWS else 'gradlew')
    props = ['-PsomCorePath=%s' % plain, '-PsomWsCorePath=%s' % ws]
    if rando:
        props.append('-PsomRandoSrc=%s' % (fetch('randomizer') / 'SoMRandomizer'))
    run([gradle, '-p', project, ':app:assembleDebug', '--no-daemon'] + props, cwd=project)
    DIST.mkdir(exist_ok=True)
    out = DIST / 'secret-of-mana-ds-thor.apk'
    shutil.copy2(project / 'app/build/outputs/apk/debug/app-debug.apk', out)
    return out


def build_librando_3ds(devkitpro, out):
    """libsomrando for the 3DS (same flags as tools/companion/n3ds/emu/build_libsomrando.sh)."""
    src = fetch('randomizer') / 'SoMRandomizer'
    lib = COMPANION / 'randomizer/libsomrando'
    gen = out / 'gen'
    obj = out / 'obj'
    gen.mkdir(parents=True, exist_ok=True)
    obj.mkdir(exist_ok=True)
    for tool, name in (('gen_p4_ev1_resources.py', 'sr_resources_p4_ev1_gen.c'), ('gen_p4_hmisc_resources.py', 'sr_p4_resources_gen.c')):
        with open(os.devnull, 'w') as quiet:
            subprocess.run([sys.executable, lib / 'tools' / tool, src, gen / name], check=True, stdout=quiet)
    sources = sorted((lib / 'src').glob('*.c')) + [lib / 'data/sr_quotes_mitch.c', lib / 'data/sr_quotes_demetri.c',
                                                  gen / 'sr_resources_p4_ev1_gen.c', gen / 'sr_p4_resources_gen.c']
    bin_dir = devkitpro / 'devkitARM/bin'
    exe = '.exe' if WINDOWS else ''
    cc = bin_dir / ('arm-none-eabi-gcc' + exe)
    flags = ['-std=gnu99', '-D_POSIX_C_SOURCE=200809L', '-O2', '-ffp-contract=off', '-march=armv6k', '-mtune=mpcore',
             '-mfloat-abi=hard', '-mtp=soft', '-ffunction-sections', '-D__3DS__', '-DSOMRANDO_ADVERTISE=1',
             '-I%s' % (COMPANION / 'launcher'), '-I%s' % (lib / 'src')]
    objects = [obj / (s.stem + '.o') for s in sources]
    with concurrent.futures.ThreadPoolExecutor(min(4, os.cpu_count() or 1)) as pool:
        for f in [pool.submit(subprocess.run, [str(cc)] + flags + ['-c', str(s), '-o', str(o)], check=True)
                  for s, o in zip(sources, objects)]:
            f.result()
    archive = out / 'libsomrando.a'
    if archive.exists():
        archive.unlink()
    run([bin_dir / ('arm-none-eabi-ar' + exe), 'rcs', archive] + objects)
    return archive


def build_3ds(rando):
    devkitpro = need('DEVKITPRO', 'install devkitPro (https://devkitpro.org/wiki/Getting_Started) with the 3ds-dev group')
    env = dict(os.environ, DEVKITARM=str(devkitpro / 'devkitARM'))
    core_src = fetch('snes9x2005')
    run(['make', '-j%d' % (os.cpu_count() or 4), 'platform=ctr'], cwd=core_src, env=env)
    core = core_src / 'snes9x2005_libretro_ctr.a'
    args = ['CORE_A=%s' % core.as_posix()]
    if rando:
        args.append('RANDO_A=%s' % build_librando_3ds(devkitpro, BUILD / 'librando-3ds').as_posix())
    n3ds = COMPANION / 'n3ds'
    # -B: the Makefile has no clean target, and a demo build left in build/ must not be reused
    run(['make', '-B', '-j%d' % (os.cpu_count() or 4)] + args, cwd=n3ds, env=env)
    DIST.mkdir(exist_ok=True)
    out = DIST / 'secret-of-mana-ds.3dsx'
    shutil.copy2(n3ds / 'som_companion.3dsx', out)
    return out


def build_pc(rando):
    ws_src = fetch('snes9x-ws')
    run(['git', 'apply', COMPANION / 'widescreen/ws_snes9x.patch'], cwd=ws_src)
    run(['make', '-j%d' % (os.cpu_count() or 4)], cwd=ws_src / 'libretro')
    core = next(p for p in (ws_src / 'libretro').glob('snes9x_libretro.*') if p.suffix in ('.so', '.dll'))
    env = dict(os.environ, SOM_PC_RELEASE=str(DIST))
    if rando:
        env['SOM_RANDO_SRC'] = str(fetch('randomizer') / 'SoMRandomizer')
    args = ['bash', COMPANION / 'pc/package_release.sh', '--bundle-sdl', '--version', 'latest', '--core', core.as_posix()]
    if WINDOWS:
        # MSYS2 MINGW64 shell: its own gcc and SDL2
        env.setdefault('CC', 'gcc')
        env.setdefault('AR', 'ar')
        env.setdefault('SDL2_PREFIX', '/mingw64')
        args += ['--platform', 'windows']
    if not rando:
        args.append('--no-generator')
    DIST.mkdir(exist_ok=True)
    run(args, env=env)
    return DIST


def in_docker(target, rando):
    run(['docker', 'build', '-t', IMAGE, ROOT / 'docker'])
    inner = ['python3', 'build.py', target] + ([] if rando else ['--no-rando'])
    user = [] if WINDOWS else ['--user', '%d:%d' % (os.getuid(), os.getgid())]
    run(['docker', 'run', '--rm'] + user + ['-e', 'HOME=/tmp', '-v', '%s:/src' % ROOT, '-w', '/src', IMAGE] + inner)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('target', choices=['thor', '3ds', 'pc', 'all'])
    parser.add_argument('--docker', action='store_true', help='build inside Docker (only Docker needs to be installed)')
    parser.add_argument('--no-rando', action='store_true', help='leave out the randomizer and Ancient Cave generator')
    args = parser.parse_args()
    targets = ['thor', '3ds', 'pc'] if args.target == 'all' else [args.target]
    rando = not args.no_rando
    if args.docker:
        in_docker(args.target, rando)
        return
    builders = {'thor': build_thor, '3ds': build_3ds, 'pc': build_pc}
    for t in targets:
        print('built', builders[t](rando))


if __name__ == '__main__':
    main()
