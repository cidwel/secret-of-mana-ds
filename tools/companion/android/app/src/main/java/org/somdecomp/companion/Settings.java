package org.somdecomp.companion;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.VibrationEffect;
import android.os.Vibrator;

/**
 * Options of the companion.  The gear and the options pages are drawn by the shared UX layer (C); this class only owns the values:
 * it persists them in the app prefs, applies them to the core and answers the layer's option taps (Native.pollEvent).
 */
final class Settings {
    static final String[] WS = {"disabled", "17", "27", "46"};
    static final String[] WS_TEXT = {"OFF", "17", "27", "46"};
    static final String[] FB = {"normal", "black"};
    static final String[] FB_TEXT = {"NORMAL", "BLACK"};
    static final String[] CORE_TEXT = {"WIDE", "PLAIN"};
    /* option ids of ux/som_ui_draw.h enum som_ux_opt */
    static final int WIDE = 0, FALLBACK = 1, HUD_HIDE = 2, CORE = 3, UI_SOUNDS = 4, HAPTICS = 5, LANGUAGE = 6, FIT = 7, SHADER = 8, FFWD = 9;
    /** Top-screen scaling (GameView): FILL CROP fills the screen with 8:7 pixels cutting the bottom rows, FIT (default) keeps the whole
     *  frame, INTEGER uses whole-number pixel factors, STRETCH fills the screen ignoring the aspect.  fullW = 256 + 2N of the chosen wide margin. */
    static final String[] FIT_TEXT = {"FIT", "FILL CROP", "INTEGER", "STRETCH"};
    /** Retro filters of GameView (0 = the automatic sharp/crisp choice of round 3). */
    static final String[] SHADER_TEXT = {"NONE", "SHARP", "SCANLINES", "LCD GRID", "CRT"};
    static volatile int fit, fullW = 348, shader;
    private static int widthOf(String ws) { return ws.equals("17") ? 290 : ws.equals("27") ? 310 : ws.equals("46") ? 348 : 256; }
    static final int EV_OPT = 1 << 16, EV_TICK = 2 << 16, EV_MAIN_MENU = 3 << 16;
    private boolean ffListen;   /* FAST FWD was tapped: the next pad key becomes the fast-forward button */

    private final SharedPreferences sp;
    static Settings current;
    private final Vibrator vib;

    Settings(Context c) {
        sp = c.getSharedPreferences("som", Context.MODE_PRIVATE);
        vib = c.getSystemService(Vibrator.class);
        current = this;
    }

    static boolean plain(Context c) { return c.getSharedPreferences("som", Context.MODE_PRIVATE).getBoolean("plain", false); }

    private int idx(String[] a, String v) { for (int i = 0; i < a.length; i++) if (a[i].equals(v)) return i; return 0; }

    /** Pushes every saved value to the core and to the UX layer (start-up). */
    void restore() {
        if (sp.getInt("defaults", 0) < 2) {   /* one-time move to the new defaults: wide 46 (all 224 rows at about 16:9) with the whole frame (FIT) */
            sp.edit().putString("ws", "46").putString("ws_on", "46").putString("fit", "FIT").putInt("defaults", 2).apply();
        }
        int ws = idx(WS, sp.getString("ws", "46")), fb = idx(FB, sp.getString("fb", "normal"));
        Native.setWidescreen(WS[ws], FB[fb]);
        fullW = widthOf(WS[ws]); fit = idx(FIT_TEXT, sp.getString("fit", "FIT"));
        Native.setOption(FIT, fit, FIT_TEXT[fit]);
        shader = Math.max(0, Math.min(SHADER_TEXT.length - 1, sp.getInt("shader", 0)));
        Native.setOption(SHADER, shader, SHADER_TEXT[shader]);
        Native.setHideHud(sp.getBoolean("hud", true));
        Native.setOption(WIDE, ws, WS_TEXT[ws]);               /* enumerated: OFF / 17 / 27 / 46 */
        Native.setOption(FALLBACK, fb, null);                   /* a toggle: on = black bars instead of the normal fallback */
        Native.setOption(HUD_HIDE, sp.getBoolean("hud", true) ? 1 : 0, null);
        Native.setOption(CORE, plain(sp) ? 1 : 0, CORE_TEXT[plain(sp) ? 1 : 0]);
        Native.setOption(UI_SOUNDS, sp.getBoolean("uisnd", true) ? 1 : 0, null);
        Native.setOption(HAPTICS, sp.getBoolean("haptic2", true) ? 1 : 0, null);
        Native.setOption(LANGUAGE, 0, "EN");
        Native.setOption(FFWD, 0, ffName());
        Native.setUiSounds(sp.getBoolean("uisnd", true));
    }

    /** Short light pulse at touch-down, when the HAPTICS option is on (default on). */
    void press() {
        if (!sp.getBoolean("haptic2", true)) return;
        try { if (vib != null && vib.hasVibrator()) vib.vibrate(VibrationEffect.createOneShot(14, 110)); } catch (RuntimeException ignored) { /* no haptics */ }
    }

    private static boolean plain(SharedPreferences sp) { return sp.getBoolean("plain", false); }

    /** Handles one event of Native.pollEvent: an option tap (apply, persist, answer) or a haptic tick. */
    void handle(int ev) {
        if (ev == EV_TICK) return;   /* the pulse now comes with the touch-down (press()) */
        if ((ev & 0xFFFF0000) != EV_OPT) return;
        int id = ev >> 8 & 0xFF, val = ev & 0xFF;
        SharedPreferences.Editor e = sp.edit();
        switch (id) {
            case WIDE: {   /* enumerated: OFF / 17 / 27 / 46 (val is the next value, wrapping) */
                int i = val % WS.length;
                e.putString("ws", WS[i]).apply();
                fullW = widthOf(WS[i]);
                Native.setWidescreen(WS[i], sp.getString("fb", "normal")); Native.setOption(id, i, WS_TEXT[i]); break;
            }
            case FALLBACK: { int i = val != 0 ? 1 : 0; e.putString("fb", FB[i]).apply(); Native.setWidescreen(sp.getString("ws", "46"), FB[i]); Native.setOption(id, i, null); break; }
            case HUD_HIDE: e.putBoolean("hud", val != 0).apply(); Native.setHideHud(val != 0); Native.setOption(id, val != 0 ? 1 : 0, null); break;
            case CORE: { int i = val % 2; e.putBoolean("plain", i == 1).apply(); Native.setOption(id, i, CORE_TEXT[i]); Native.toast("RESTART THE APP"); break; }
            case UI_SOUNDS: e.putBoolean("uisnd", val != 0).apply(); Native.setUiSounds(val != 0); Native.setOption(id, val != 0 ? 1 : 0, null); break;
            case HAPTICS: e.putBoolean("haptic2", val != 0).apply(); Native.setOption(id, val != 0 ? 1 : 0, null); break;
            case FIT: { int i = val % FIT_TEXT.length; fit = i; e.putString("fit", FIT_TEXT[i]).apply(); Native.setOption(id, i, FIT_TEXT[i]); break; }
            case SHADER: { int i = val % SHADER_TEXT.length; shader = i; e.putInt("shader", i).apply(); Native.setOption(id, i, SHADER_TEXT[i]); break; }
            case FFWD:   /* first tap: wait for a key; a tap while waiting clears the button */
                if (ffListen) { ffListen = false; e.putInt("ffkey", 0).apply(); Native.setFastForward(false); }
                else ffListen = true;
                Native.setOption(FFWD, 0, ffListen ? "PRESS KEY" : ffName());
                break;
            default: Native.setOption(LANGUAGE, 0, "EN"); break;
        }
    }

    private String ffName() {
        int k = sp.getInt("ffkey", 0);
        return k == 0 ? "NONE" : android.view.KeyEvent.keyCodeToString(k).replace("KEYCODE_BUTTON_", "").replace("KEYCODE_", "").replace('_', ' ');
    }

    /** Pad key hook: binds the fast-forward button while waiting for one, and holds 3x speed while it is down.  True when the key is used up here. */
    boolean fastForwardKey(android.view.KeyEvent ev) {
        int code = ev.getKeyCode(), act = ev.getAction();
        if (act == android.view.KeyEvent.ACTION_MULTIPLE) return false;
        if (ffListen) {
            if (act == android.view.KeyEvent.ACTION_DOWN && ev.getRepeatCount() == 0) {
                ffListen = false;
                sp.edit().putInt("ffkey", code).apply();
                Native.setOption(FFWD, 0, ffName());
            }
            return true;
        }
        if (code != sp.getInt("ffkey", 0)) return false;
        Native.setFastForward(act == android.view.KeyEvent.ACTION_DOWN);
        return true;
    }
}
