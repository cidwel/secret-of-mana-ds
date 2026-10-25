#!/usr/bin/env python3
"""Secret of Mana DS: give it your ROM and it gets the game ready for the AYN Thor, a Nintendo 3DS or the PC.

usage: python3 som-ds.py                                        (window)
       python3 som-ds.py ROM --target 3ds  --out SD_CARD_ROOT
       python3 som-ds.py ROM --target thor [--out FOLDER] [--no-adb]
       python3 som-ds.py ROM --target pc   --out FOLDER
       add --build to compile from source (build.py) instead of downloading the release

3ds   copies secret-of-mana-ds.3dsx to <SD>/3ds/secret-of-mana-ds/ and your ROM to <SD>/som/rom.sfc.
thor  with adb and the Thor connected over USB: installs the APK and copies the ROM into the app's folder.
      Otherwise writes the APK and rom.sfc to FOLDER, to copy by hand (the app asks for the ROM on first start).
pc    unpacks the PC version into FOLDER with your ROM and a start script.
The ROM is only copied to your own devices and folders; nothing is uploaded.
"""
import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import tarfile
import tempfile
import threading
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent
DIST = ROOT / 'dist'
REPO = 'cidwel/secret-of-mana-ds'
PACKAGE = 'org.somdecomp.companion'
US_SHA256 = '4c15013131351e694e05f22e38bb1b3e4031dedac77ec75abecebe8520d82d5f'
ROM_SIZE = 2 * 1024 * 1024
ASSETS = {'thor': 'secret-of-mana-ds-thor.apk', '3ds': 'secret-of-mana-ds.3dsx',
          'pc-linux': 'som-pc-latest-linux-x86_64.tar.gz', 'pc-windows': 'som-pc-latest-windows-x86_64.zip'}


def read_rom(path):
    """The ROM without a copier header, and whether it is the US v1.0 image the apps are made for."""
    data = pathlib.Path(path).read_bytes()
    if len(data) == ROM_SIZE + 512:
        data = data[512:]
    if len(data) != ROM_SIZE:
        raise ValueError('%s is not a Secret of Mana ROM (%d bytes, expected 2 MiB)' % (path, len(data)))
    return data, hashlib.sha256(data).hexdigest() == US_SHA256


def pc_asset():
    if sys.platform.startswith('win'):
        return 'pc-windows'
    if sys.platform.startswith('linux'):
        return 'pc-linux'
    raise SystemExit('The PC version is built for Linux and Windows only; on a Mac use the Thor or 3DS targets')


def binary(kind, build, log):
    """Path of a built app: dist/ from build.py, or the asset of the latest GitHub release."""
    name = ASSETS[kind]
    local = DIST / name
    if build:
        target = 'pc' if kind.startswith('pc') else kind
        log('Compiling %s from source (this takes a while the first time)...' % target)
        subprocess.run([sys.executable, str(ROOT / 'build.py'), target], check=True)
    if local.is_file():
        return local
    if build:
        raise SystemExit('build.py did not produce %s' % local)
    log('Downloading %s from the latest release of %s...' % (name, REPO))
    with urllib.request.urlopen('https://api.github.com/repos/%s/releases/latest' % REPO) as r:
        release = json.load(r)
    url = next((a['browser_download_url'] for a in release['assets'] if a['name'] == name), None)
    if not url:
        raise SystemExit('The latest release has no %s; use --build to compile it' % name)
    out = pathlib.Path(tempfile.mkdtemp()) / name
    with urllib.request.urlopen(url) as r, open(out, 'wb') as f:
        shutil.copyfileobj(r, f)
    return out


def adb(*args):
    return subprocess.run(['adb'] + list(args), capture_output=True, text=True)


def thor_connected():
    if not shutil.which('adb'):
        return False
    lines = adb('devices').stdout.splitlines()[1:]
    return any(l.strip().endswith('device') for l in lines)


def prepare(rom_path, target, out, build=False, use_adb=True, force=False, log=print):
    rom, is_us = read_rom(rom_path)
    if not is_us:
        if not force:
            raise SystemExit('This ROM is not the US v1.0 image the apps are made for (use --force to copy it anyway)')
        log('Warning: not the US v1.0 ROM; some features may not work')
    if target == '3ds':
        sd = pathlib.Path(out)
        app = binary('3ds', build, log)
        (sd / '3ds/secret-of-mana-ds').mkdir(parents=True, exist_ok=True)
        (sd / 'som').mkdir(parents=True, exist_ok=True)
        shutil.copy2(app, sd / '3ds/secret-of-mana-ds/secret-of-mana-ds.3dsx')
        (sd / 'som/rom.sfc').write_bytes(rom)
        log('Done. Put the SD card in the 3DS and start Secret of Mana DS from the Homebrew Launcher.')
    elif target == 'thor':
        apk = binary('thor', build, log)
        if use_adb and thor_connected():
            log('Installing on the connected device...')
            r = adb('install', '-r', str(apk))
            if r.returncode:
                raise SystemExit('adb install failed: ' + (r.stderr or r.stdout))
            with tempfile.TemporaryDirectory() as tmp:
                p = pathlib.Path(tmp) / 'rom.sfc'
                p.write_bytes(rom)
                folder = '/sdcard/Android/data/%s/files' % PACKAGE
                adb('shell', 'mkdir', '-p', folder)
                r = adb('push', str(p), folder + '/rom.sfc')
            if r.returncode:
                log('The ROM could not be copied (%s); the app will ask for it on first start.' % (r.stderr or r.stdout).strip())
            log('Done. Open Secret of Mana DS on the Thor.')
        else:
            dest = pathlib.Path(out or 'secret-of-mana-ds-thor')
            dest.mkdir(parents=True, exist_ok=True)
            shutil.copy2(apk, dest / 'secret-of-mana-ds-thor.apk')
            (dest / 'rom.sfc').write_bytes(rom)
            log('Done: %s. Copy both files to the Thor, install the APK and pick rom.sfc when the app asks.' % dest)
    elif target == 'pc':
        kind = pc_asset()
        pack = binary(kind, build, log)
        dest = pathlib.Path(out)
        dest.mkdir(parents=True, exist_ok=True)
        if pack.suffix == '.zip':
            zipfile.ZipFile(pack).extractall(dest)
        else:
            with tarfile.open(pack) as t:
                t.extractall(dest, filter='data')
        home = next(dest.glob('som-pc-*'))
        (home / 'rom.sfc').write_bytes(rom)
        if kind == 'pc-windows':
            (home / 'start.bat').write_text('@echo off\r\ncd /d "%~dp0"\r\nsom-pc.exe --menu --rom rom.sfc\r\n')
        else:
            script = home / 'start.sh'
            script.write_text('#!/bin/sh\ncd "$(dirname "$0")" && exec ./som-pc --menu --rom rom.sfc\n')
            script.chmod(0o755)
        log('Done: run start%s in %s.' % ('.bat' if kind == 'pc-windows' else '.sh', home))
    else:
        raise SystemExit('unknown target ' + target)


def window():
    import tkinter as tk
    from tkinter import filedialog, ttk

    root = tk.Tk()
    root.title('Secret of Mana DS')
    root.resizable(False, False)
    frame = ttk.Frame(root, padding=16)
    frame.grid()
    rom, out, target = tk.StringVar(), tk.StringVar(), tk.StringVar(value='thor')
    build, use_adb = tk.BooleanVar(), tk.BooleanVar(value=True)

    ttk.Label(frame, text='Secret of Mana DS', font=('TkDefaultFont', 16, 'bold')).grid(columnspan=3, sticky='w')
    ttk.Label(frame, text='Your ROM is copied only to your own device or folder.').grid(columnspan=3, sticky='w', pady=(0, 12))
    ttk.Label(frame, text='ROM (US v1.0)').grid(row=2, column=0, sticky='w')
    ttk.Entry(frame, textvariable=rom, width=48).grid(row=2, column=1, padx=6)
    ttk.Button(frame, text='Choose...', command=lambda: rom.set(filedialog.askopenfilename(
        filetypes=[('SNES ROM', '*.sfc *.smc'), ('All files', '*')]) or rom.get())).grid(row=2, column=2)
    ttk.Label(frame, text='Device').grid(row=3, column=0, sticky='w', pady=8)
    devices = ttk.Frame(frame)
    devices.grid(row=3, column=1, columnspan=2, sticky='w')
    for value, label in (('thor', 'AYN Thor (Android)'), ('3ds', 'Nintendo 3DS'), ('pc', 'PC')):
        ttk.Radiobutton(devices, text=label, value=value, variable=target).pack(side='left', padx=(0, 12))
    hint = ttk.Label(frame, text='')
    hint.grid(row=4, column=1, columnspan=2, sticky='w')
    ttk.Label(frame, text='Destination').grid(row=5, column=0, sticky='w')
    ttk.Entry(frame, textvariable=out, width=48).grid(row=5, column=1, padx=6)
    ttk.Button(frame, text='Choose...', command=lambda: out.set(filedialog.askdirectory() or out.get())).grid(row=5, column=2)
    ttk.Checkbutton(frame, text='Install over USB when the Thor is connected (adb)', variable=use_adb).grid(
        row=6, column=1, columnspan=2, sticky='w', pady=(8, 0))
    ttk.Checkbutton(frame, text='Compile from source instead of downloading the release', variable=build).grid(
        row=7, column=1, columnspan=2, sticky='w')
    text = tk.Text(frame, width=72, height=8, state='disabled')
    text.grid(row=9, columnspan=3, pady=(12, 0))

    def on_target(*_):
        hint.config(text={'thor': 'Destination is only used when the Thor is not connected.',
                          '3ds': 'Destination: the root of the 3DS SD card.',
                          'pc': 'Destination: the folder to install the PC version in.'}[target.get()])
    target.trace_add('write', on_target)
    on_target()

    def log(msg):
        def put():
            text.config(state='normal')
            text.insert('end', msg + '\n')
            text.see('end')
            text.config(state='disabled')
        root.after(0, put)

    def go():
        if not rom.get():
            log('Choose your ROM first.')
            return
        if target.get() != 'thor' and not out.get():
            log('Choose the destination first.')
            return
        button.config(state='disabled')

        def work():
            try:
                prepare(rom.get(), target.get(), out.get() or None, build.get(), use_adb.get(), log=log)
            except (SystemExit, Exception) as e:
                log('Error: %s' % e)
            finally:
                root.after(0, lambda: button.config(state='normal'))
        threading.Thread(target=work, daemon=True).start()

    button = ttk.Button(frame, text='Get it ready', command=go)
    button.grid(row=8, column=1, sticky='e', pady=(12, 0))
    root.mainloop()


def main():
    if len(sys.argv) == 1:
        window()
        return
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('rom')
    parser.add_argument('--target', required=True, choices=['thor', '3ds', 'pc'])
    parser.add_argument('--out', help='3DS: SD card root; Thor: folder when no device is connected; PC: install folder')
    parser.add_argument('--build', action='store_true', help='compile from source with build.py instead of downloading')
    parser.add_argument('--no-adb', action='store_true', help='Thor: write the files to --out even if a device is connected')
    parser.add_argument('--force', action='store_true', help='accept a ROM that is not the US v1.0 image')
    args = parser.parse_args()
    if args.target in ('3ds', 'pc') and not args.out:
        parser.error('--out is required for %s' % args.target)
    prepare(args.rom, args.target, args.out, args.build, not args.no_adb, args.force)


if __name__ == '__main__':
    main()
