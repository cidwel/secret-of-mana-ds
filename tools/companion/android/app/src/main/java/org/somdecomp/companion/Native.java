package org.somdecomp.companion;

import android.graphics.Bitmap;

/** JNI bridge to libsomjni.so (libretro host + som_companion + status renderer). */
final class Native {
    static { System.loadLibrary("somjni"); }

    /** Loads the libretro core at corePath, or only the demo state when corePath is null. Returns a status string. */
    static native String init(String corePath, String systemDir);
    static native String loadRom(byte[] rom);
    /** Loads a libretro save state made by the same core build for the same ROM. Returns a status string. */
    static native String loadState(byte[] state);
    static native void start();
    static native void stop();
    static native void setPad(int mask);
    /** Tap on the bottom screen at (x, y) of a w x h bitmap; the loop thread turns it into a command or a pad tap. */
    static native void touchUx(int x, int y, int w, int h);
    /** Pad panel mode: 1..4 move the focus ring (up, down, left, right), 5 presses the focused target, 6 is back, 7 toggles the mode. */
    static native void navRequest(int req);
    static native boolean navOn();
    /** Draws the context-aware bottom screen at the bitmap size. */
    static native boolean renderUx(Bitmap bmp);
    /** Touch-down / touch-up feedback (pressed look, click sound); coordinates as in touchUx. */
    static native boolean pressUx(int x, int y, int w, int h, boolean down);
    static native void moveUx(int x, int y, int w, int h);
    /** Optional graphics pack (som_ui_pack.bin); without it the renderer uses built-in art. */
    static native void loadPack(byte[] pack);
    static native void loadSprites(byte[] pack);
    static native double uxDrawMs();
    /** Long press (about 0.5 s) on the bottom screen: the UX layer arms an entry for the next slot tap or opens the slot editor. */
    static native void longPressUx(int x, int y, int w, int h);
    /* ---- button bindings (shared engine som_bind.c) */
    static native int bindKeyInput(int keyCode);              /* shared input id of an Android key code, 0 none */
    static native int bindGameMask(int inputs);                /* libretro pad mask of the inputs the game may see */
    static native int bindStick(int prevDirs, float x, float y);
    static native boolean bindTrigger(boolean prev, float v);
    /** Feeds the held inputs (bit i-1 = input id i); returns the swallow mask (low 32 bits). */
    static native long bindUpdate(int held, long nowMs);
    static native void bindLoad(String text);
    /** UI sounds on the bottom screen (the game's own menu effects). */
    static native void setUiSounds(boolean on);
    /** Events from the UX layer: -1 none, 0x1iiVV option tap (id, value), 0x20000 haptic tick. */
    static native int pollEvent();
    static native void setOption(int id, int value, String text);
    static native String quickSave();                          /* quick slots, one "slot kind id" per line */
    static native void quickLoad(String text);
    static native boolean quickDirty();
    static native String gameKey();                            /* hash of the three hero names, "" while unknown (title, name entry) */
    static native int gameEvent();                             /* one-shot: 1 a new game began, 2 a saved game was loaded, 3 a hero joined (names changed, same game), 0 none */
    static native int seenGet();                               /* discovered weapons / spirits / items, packed */
    static native void seenSet(int packed);
    static native void quickClear();
    static native String nameConfirmed();                      /* one-shot "hero NAME" when a name entry was confirmed (A..Z names only), "" none */
    static native void setLastName(int hero, String name);     /* LAST NAME key of the name entry: the last confirmed name of hero 0..2 ("" none) */
    /** Shows a short message in the bottom screen strip. */
    static native void toast(String s);
    static native String romVariant();
    /** Battery save and save states belong to a game: base = key of the original or of the generated run, sig = hash of the gameplay options ("" none).  Call before loadRom. */
    static native void setSaveKeys(String base, String sig);
    /** Writes the cartridge RAM when it changed (pause, stop). */
    static native void flushSave();
    /** Save-state slots 0..7 (-1 = the remembered slot); the result is 0 or a negative som_states code. */
    static native int stateSave(int slot);
    static native int stateLoad(int slot);
    static native int stateStep(int delta);
    /** Text of the in-game credits page (lines separated by newlines, "# " headings). */
    static native void setCredits(String text);
    /** Copies the last game frame (always the whole frame) into bmp; returns false when there is none. */
    static native boolean copyFrame(Bitmap bmp);
    /** Hide the HUD (core BG3 layer) during free field play; the frame geometry never changes. Default on. */
    static native void setHideHud(boolean on);
    static native void setFastForward(boolean on);           /* 3x speed while true, extra frames muted */
    /** Widescreen core options: ws = disabled|17|27|46, fb = normal|black (ignored by the plain core). */
    static native void setWidescreen(String ws, String fb);
    /** Last 2-second performance line (core ms/frame, input latency, audio underruns, HUD-mask state, HP/MP). */
    static native String stats();
    /** Debug builds only: writes bytes into WRAM (test fixtures). */
    static native void poke(int addr, byte[] data);
    static native void pokeHold(int addr, byte[] data, int frames);   /* test hook: rewrite every frame, 0 releases */
    static native int frameWidth();
    static native int frameHeight();
    static native String ringInfo();
    static native void debugCmd(int kind, int hero, int arg, int spell, int targetKind, int targetSlot);
    static native void stateDump();
    public static native String probe(int frames, int hold);
}
