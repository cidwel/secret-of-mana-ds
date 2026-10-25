package org.somdecomp.companion;

import android.graphics.Bitmap;

/** JNI bridge to the start menu model and renderer (tools/companion/launcher) and to the generator seam (somrando_api.h). */
final class LauncherNative {
    static { System.loadLibrary("somjni"); }

    /* sl_action_kind */
    static final int ACT_NONE = 0, ACT_PLAY_ORIGINAL = 1, ACT_GENERATE = 2, ACT_CANCEL = 3, ACT_PLAY = 4, ACT_QUIT = 5, ACT_DELETE_SAVED = 6;
    /* sl_button */
    static final int B_UP = 0, B_DOWN = 1, B_LEFT = 2, B_RIGHT = 3, B_A = 4, B_B = 5, B_X = 6, B_Y = 7, B_L = 8, B_R = 9, B_START = 10, B_SELECT = 11;
    /* somrando_api.h */
    static final int RANDO_OK = 0, RANDO_UNAVAILABLE = 1, RANDO_FAILED = 2, RANDO_CANCELLED = 3, RANDO_BAD_ROM = 4;

    /** Progress callback of nativeGenerate; return true to cancel. Called on the generator thread. */
    interface Progress { boolean onProgress(int step, int total, String name); }

    static native void init(long entropy);
    static native void loadPack(byte[] pack);
    static native void setStatus(String original, String generator);
    static native void setView(int w, int h);
    static native void key(int button);
    static native void tap(int x, int y, int w, int h);
    static native void scroll(int rows);
    static native void tick();
    static native int poll();
    static native int revision();
    static native int screen();
    static native int snapshot();
    static native int rowHeightPx(int w, int h);
    static native boolean render(Bitmap bmp);
    /** The top screen: wallpaper on the main menu, otherwise the summary of the game being set up. */
    static native boolean renderTop(Bitmap bmp);
    /** Changes only when the top screen would look different. */
    static native int topRevision();

    /* saved randomized games (at most four) */
    static native void savedClear();
    static native void savedHidden(int n);
    static native int savedAdd(String key, String preset, String mode, String seed, String difficulty, String date, String path, String lines);
    static native int savedCount();
    static native String savedKey(int slot);
    static native int actArg();

    static native String configJson();
    static native String configLines();
    static native String configKey();
    /** 0 mode key, 1 seed, 2 difficulty ("" when the mode has none). */
    static native String configField(int which);
    static native String romPath();

    static native void genCached(String path);
    static native void genBegin();
    static native void genProgress(int step, int total, String name);
    static native void genDone(String path);
    static native void genFail(String message);
    static native void genUnavailable(String message);

    /* gameplay tweaks and credits pages (X and Y on the main screen) */
    static native int pagesInit(String menuJson, String valuesJson, String creditsJson);
    static native int pageActive();
    static native boolean gameplayDirty();
    static native boolean gameplayIsDefault();
    static native String gameplayJson();
    static native String creditsText(String creditsJson);
    /** Patches the ROM file `in` into `out`; "" on success (skipped[0] lists the options that conflicted with the ROM), else the error. */
    static native String gameplayApply(String in, String optionsJson, String out, String[] skipped);

    static native boolean generatorAvailable();
    /** Why the current configuration cannot be generated on the device ("" when it can). */
    static native String generatorWhy();
    static native int presetSupported(int preset);
    static native void refreshSupport();
    static native void refreshConfigSupport();
    static native String generatorVersion();
    static native int nativeGenerate(String mode, String seed, String difficulty, String lines, String src, String dst, String workdir,
                                     Progress cb, String[] errOut);
}
