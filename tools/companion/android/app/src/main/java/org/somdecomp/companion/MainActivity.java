package org.somdecomp.companion;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Gravity;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;

/**
 * Game on the default display, status/touch screen on a presentation display when one exists (AYN Thor), else
 * stacked below the game on the single screen.  The ROM is the user's: picked with the system file chooser and
 * kept in memory only.
 */
public final class MainActivity extends Activity {
    private static final int PICK_ROM = 1;
    private StatusPresentation presentation;
    private TextView info;
    private int keys, dbg;

    private boolean redirected;

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        /* The game must be on the default (top) display and the panel on the presentation display.  A launcher can start
         * us on the bottom display (AYN Thor: Screen-2); then start a fresh instance on display 0 and leave. */
        Display here = getDisplay();
        if (here != null && here.getDisplayId() != Display.DEFAULT_DISPLAY) {
            redirected = true;
            Intent i = new Intent(getIntent()).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
            ActivityOptions o = ActivityOptions.makeBasic();
            o.setLaunchDisplayId(Display.DEFAULT_DISPLAY);
            startActivity(i, o.toBundle());
            finish();
            return;
        }
        File ws = new File(getApplicationInfo().nativeLibraryDir, "libsnes9x_ws_core.so");
        File core = ws.exists() && !Settings.plain(this) ? ws : new File(getApplicationInfo().nativeLibraryDir, "libsnes9x_core.so");
        String msg = Native.init(core.exists() ? core.getPath() : null, getFilesDir().getPath());
        try (InputStream pk = getAssets().open("som_ui_pack.bin")) { Native.loadPack(readAll(pk)); } catch (Exception ignored) { /* no pack: built-in art */ }
        try (InputStream pk = getAssets().open("som_enemy_sprites.bin")) { Native.loadSprites(readAll(pk)); } catch (Exception ignored) { /* no sprite pack: live render only */ }

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        FrameLayout stack = new FrameLayout(this);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        info = new TextView(this);
        info.setTextColor(0xFFFFFFFF);
        info.setBackgroundColor(0x80000000);
        info.setTextSize(11);
        info.setText(msg + "  (tap the text to choose a ROM)");
        info.setOnClickListener(v -> pickRom());
        root.addView(new GameView(this), new LinearLayout.LayoutParams(-1, 0, 1f));
        if (secondDisplay() == null) {
            root.addView(new StatusView(this), new LinearLayout.LayoutParams(-1, 0, 0.6f));
        }
        stack.addView(root, new FrameLayout.LayoutParams(-1, -1));
        stack.addView(info, new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.START));
        setContentView(stack);
        WindowInsetsController wic = getWindow().getInsetsController();
        if (wic != null) {
            wic.hide(WindowInsets.Type.systemBars());
            wic.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        }
        info.postDelayed(new Runnable() {
            @Override public void run() {
                String st = Native.stats();   /* the core is running: the ROM message goes away, the performance line goes to logcat only */
                if (!st.isEmpty()) { info.setVisibility(android.view.View.GONE); android.util.Log.d("somcompanion", st); }
                info.postDelayed(this, st.isEmpty() ? 1000 : 10000);
            }
        }, 1000);
        /* start menu: rom_path is a generated ROM chosen there; otherwise the shortcut below applies */
        String menuRom = getIntent().getStringExtra("rom_path");
        if (getIntent().hasExtra("base_key")) Native.setSaveKeys(getIntent().getStringExtra("base_key"), getIntent().getStringExtra("gp_sig"));
        try (InputStream cr = getAssets().open("credits.json")) { Native.setCredits(LauncherNative.creditsText(new String(readAll(cr), "UTF-8"))); } catch (Throwable ignored) { /* no credits asset */ }
        File dev = menuRom != null ? new File(menuRom) : new File(getExternalFilesDir(null), "rom.sfc");   /* shortcut: adb push the ROM here to skip the picker */
        if (dev.exists()) {
            try (InputStream in = new java.io.FileInputStream(dev)) { info.setText(Native.loadRom(readAll(in))); }
            catch (Exception e) { info.setText("ROM read failed: " + e); }
        }
        /* Test profile: a save state pushed as state.sav next to rom.sfc is loaded at start and by the LOAD STATE button. */
        File stf = new File(getExternalFilesDir(null), "state.sav");
        if (menuRom == null && stf.exists()) {
            loadStateFile(stf);
            TextView btn = new TextView(this);
            btn.setText("  LOAD TEST STATE  ");
            btn.setTextColor(0xFFFFFFFF);
            btn.setBackgroundColor(0xA0204080);
            btn.setTextSize(11);
            btn.setOnClickListener(v -> loadStateFile(stf));
            stack.addView(btn, new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.END));
        }
        if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0) registerDebugHook();
        settings = new Settings(this);
        settings.restore();
        binds = Bindings.instance(this);
        binds.load();
        /* adb: am start --ez hide_hud false / --es ws 46 / --es fb black override the saved options for this run */
        if (getIntent().hasExtra("hide_hud")) Native.setHideHud(getIntent().getBooleanExtra("hide_hud", true));
        if (getIntent().hasExtra("ws") || getIntent().hasExtra("fb"))
            Native.setWidescreen(getIntent().getStringExtra("ws") != null ? getIntent().getStringExtra("ws") : "46",
                                 getIntent().getStringExtra("fb") != null ? getIntent().getStringExtra("fb") : "normal");
        if (getIntent().hasExtra("ws")) Settings.fullW = "17".equals(getIntent().getStringExtra("ws")) ? 290 : "27".equals(getIntent().getStringExtra("ws")) ? 310 : "46".equals(getIntent().getStringExtra("ws")) ? 348 : 256;
        if (getIntent().hasExtra("fit")) Settings.fit = Math.max(0, java.util.Arrays.asList(Settings.FIT_TEXT).indexOf(getIntent().getStringExtra("fit")));   /* adb: --es fit FILL|FIT|INTEGER|STRETCH */
        if (getIntent().hasExtra("shader")) Settings.shader = Math.max(0, java.util.Arrays.asList(Settings.SHADER_TEXT).indexOf(getIntent().getStringExtra("shader")));   /* adb: --es shader SCANLINES|... */
        Native.start();
        if (getIntent().getStringExtra("gp_skipped") != null) Native.toast("NOT APPLIED: " + getIntent().getStringExtra("gp_skipped").toUpperCase(java.util.Locale.ROOT).replace(',', ' '));
        info.postDelayed(events, 60);
    }

    /** adb shell am broadcast -a org.somdecomp.companion.DEBUG --ei pad 0x10 --ei ms 500 | --es poke "CC48 6024" */
    private void registerDebugHook() {
        BroadcastReceiver r = new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent i) {
                if (i.hasExtra("fit")) Settings.fit = Math.max(0, java.util.Arrays.asList(Settings.FIT_TEXT).indexOf(i.getStringExtra("fit")));   /* test hook */
                String hold = i.getStringExtra("pokehold");   /* "ADDR HEX FRAMES": keep rewriting (a status the game would clear) */
                if (hold != null) {
                    String[] p = hold.trim().split("\\s+");
                    byte[] d = new byte[p[1].length() / 2];
                    for (int k = 0; k < d.length; k++) d[k] = (byte) Integer.parseInt(p[1].substring(2 * k, 2 * k + 2), 16);
                    Native.pokeHold(Integer.parseInt(p[0], 16), d, Integer.parseInt(p[2]));
                }
                String poke = i.getStringExtra("poke");
                if (poke != null) {
                    String[] p = poke.trim().split("\\s+");
                    int addr = Integer.parseInt(p[0], 16);
                    byte[] d = new byte[p[1].length() / 2];
                    for (int k = 0; k < d.length; k++) d[k] = (byte) Integer.parseInt(p[1].substring(2 * k, 2 * k + 2), 16);
                    Native.poke(addr, d);
                }
                if (i.getStringExtra("quick") != null) Native.quickLoad(i.getStringExtra("quick"));   /* "slot kind id" lines, test hook */
                if (i.hasExtra("ringq")) Native.ringInfo();
                if (i.hasExtra("state")) Native.stateDump();
                if (i.hasExtra("cmd")) { String[] q = i.getStringExtra("cmd").split(","); Native.debugCmd(Integer.parseInt(q[0]), Integer.parseInt(q[1]), Integer.parseInt(q[2]), Integer.parseInt(q[3]), q.length > 4 ? Integer.parseInt(q[4]) : 0, q.length > 5 ? Integer.parseInt(q[5]) : 0); }
                if (i.hasExtra("probe")) new Thread(() -> Native.probe(i.getIntExtra("probe", 60), i.getIntExtra("hold", 10))).start();
                int pad = i.getIntExtra("pad", 0), ms = i.getIntExtra("ms", 100);
                if (pad != 0) {
                    dbg |= pad;
                    Native.setPad(keys | dbg);
                    info.postDelayed(() -> { dbg &= ~pad; Native.setPad(keys | dbg); }, ms);
                }
            }
        };
        registerReceiver(r, new IntentFilter("org.somdecomp.companion.DEBUG"), Context.RECEIVER_EXPORTED);
    }

    private void loadStateFile(File f) {
        try (InputStream in = new java.io.FileInputStream(f)) { info.setText(Native.loadState(readAll(in))); }
        catch (Exception e) { info.setText("state read failed: " + e); }
    }

    private static byte[] readAll(InputStream in) throws java.io.IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buf = new byte[1 << 16];
        for (int n; (n = in.read(buf)) > 0; ) out.write(buf, 0, n);
        return out.toByteArray();
    }

    private Display secondDisplay() {
        DisplayManager dm = (DisplayManager) getSystemService(Context.DISPLAY_SERVICE);
        Display[] ds = dm.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
        return ds.length > 0 ? ds[0] : null;
    }

    /* The second screen can come and go while the game runs (display mode switch, dock, cable): show the panel
     * again when it returns and drop the dead presentation when it leaves. */
    private final DisplayManager.DisplayListener displayListener = new DisplayManager.DisplayListener() {
        @Override public void onDisplayAdded(int id) { showPanel(); }
        @Override public void onDisplayChanged(int id) { }
        @Override public void onDisplayRemoved(int id) {
            if (presentation != null && presentation.getDisplay() != null && presentation.getDisplay().getDisplayId() == id) {
                try { presentation.dismiss(); } catch (RuntimeException ignored) { }
                presentation = null;
            }
        }
    };

    private void showPanel() {
        if (redirected) return;
        Display d = secondDisplay();
        if (d != null && presentation == null) {
            presentation = new StatusPresentation(this, d);
            try { presentation.show(); } catch (RuntimeException e) { presentation = null; }
        }
    }

    @Override protected void onStart() {
        super.onStart();
        if (redirected) return;
        ((DisplayManager) getSystemService(Context.DISPLAY_SERVICE)).registerDisplayListener(displayListener, null);
        showPanel();
    }

    @Override protected void onPause() {
        if (!redirected) Native.flushSave();   /* cartridge RAM reaches the disk whenever the game leaves the screen */
        super.onPause();
    }

    @Override protected void onStop() {
        if (redirected) { super.onStop(); return; }
        Native.flushSave();
        ((DisplayManager) getSystemService(Context.DISPLAY_SERVICE)).unregisterDisplayListener(displayListener);
        if (presentation != null) { presentation.dismiss(); presentation = null; }
        super.onStop();
    }

    /** Options > MAIN MENU: the game in progress ends (battery save kept) and the app starts over at the start menu.  The process is restarted so
     *  the core and the bottom-screen layer begin clean, as on a cold launch. */
    private void backToStartMenu() {
        Native.flushSave();
        Native.stop();
        redirected = true;   /* onDestroy: already stopped */
        startActivity(new Intent(this, LaunchActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK));
        finish();
        Runtime.getRuntime().exit(0);
    }

    @Override protected void onDestroy() {
        if (!redirected) { Native.flushSave(); Native.stop(); }
        super.onDestroy();
    }

    private void pickRom() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*");
        startActivityForResult(i, PICK_ROM);
    }

    @Override protected void onActivityResult(int req, int res, Intent data) {
        if (req != PICK_ROM || res != RESULT_OK || data == null) return;
        Uri u = data.getData();
        try (InputStream in = getContentResolver().openInputStream(u)) {
            info.setText(Native.loadRom(readAll(in)));
            binds.load();
        } catch (Exception e) {
            info.setText("ROM read failed: " + e);
        }
    }

    /* ---- inputs: every physical input becomes a bit of `inMask` (shared ids of som_bind.h); the shared binding engine decides
     * which of them fire a hotkey and which are hidden from the game, and maps the rest to the SNES pad
     * (docs/companion-bindings-20261017.md). */
    private Bindings binds;
    private Settings settings;
    private final Runnable events = new Runnable() {   /* options, hotlink persistence and haptic ticks of the UX layer */
        @Override public void run() {
            for (int ev; (ev = Native.pollEvent()) != -1; ) {
                if (ev == Settings.EV_MAIN_MENU) { backToStartMenu(); return; }
                if (settings != null) settings.handle(ev);
            }
            if (binds != null) binds.saveIfDirty();
            info.postDelayed(this, 60);
        }
    };
    private int keyMask, axisMask, hatMask, stickL, stickR;
    private long inputQuietUntil;   /* uptime ms: key presses before it are ignored (launcher press still travelling) */

    /* A button held in the launcher (or released while we were away) must never reach the game as a press: drop all
     * held state when focus changes and ignore fresh presses for a short quiet period after focus returns. */
    @Override public void onWindowFocusChanged(boolean focus) {
        super.onWindowFocusChanged(focus);
        keyMask = axisMask = hatMask = 0; stickL = stickR = 0; ltDown = rtDown = false;
        inputQuietUntil = android.os.SystemClock.uptimeMillis() + (focus ? 600 : 0);
        android.util.Log.i("somkeys", "focus " + focus + ": input state cleared");
        refreshInputs();
    }
    private boolean ltDown, rtDown;

    /* Save states on the pad: hold R3, then R1 saves, L1 loads, R2 selects the next slot and L2 the previous one (the current slot is shown on the
     * bottom screen's SAVE STATES page).  While R3 is held the chord buttons and R3 itself are hidden from the game and from the binding engine.
     * There is no on-screen message: success is the haptic tick of the core, a failure shows a short error. */
    private static final int R3 = 1 << 11, C_L1 = 1 << 4, C_R1 = 1 << 5, C_L2 = 1 << 6, C_R2 = 1 << 7;
    private int chordPrev;
    private final java.util.concurrent.ExecutorService stateIo = java.util.concurrent.Executors.newSingleThreadExecutor();

    private int stateChord(int held) {
        if ((held & R3) == 0) { chordPrev = 0; return held; }
        int now = held & (C_L1 | C_R1 | C_L2 | C_R2), edge = now & ~chordPrev;
        chordPrev = now;
        if ((edge & C_R1) != 0) stateAction(0);
        if ((edge & C_L1) != 0) stateAction(1);
        if ((edge & C_R2) != 0) stateAction(2);
        if ((edge & C_L2) != 0) stateAction(3);
        return held & ~(R3 | C_L1 | C_R1 | C_L2 | C_R2);
    }

    private void stateAction(int what) {
        stateIo.execute(() -> {
            int rc = 0;
            if (what == 0) rc = Native.stateSave(Native.stateStep(0));
            else if (what == 1) { rc = Native.stateLoad(Native.stateStep(0)); if (rc == -2) rc = 0; }   /* an empty slot: nothing to load, nothing to say */
            else Native.stateStep(what == 2 ? 1 : -1);
            if (rc != 0) Native.toast(what == 0 ? "SAVE FAILED" : "LOAD FAILED");
        });
    }
    private int lastHeld, lastSwallow;
    private final Runnable tick = new Runnable() {
        @Override public void run() { refreshInputs(); }
    };

    private static boolean gamepadKey(int c) {
        return (c >= KeyEvent.KEYCODE_BUTTON_A && c <= KeyEvent.KEYCODE_BUTTON_MODE) || (c >= KeyEvent.KEYCODE_DPAD_UP && c <= KeyEvent.KEYCODE_DPAD_RIGHT)
                || c == KeyEvent.KEYCODE_DPAD_CENTER;
    }

    private static int bitOf(int input) { return input > 0 ? 1 << (input - 1) : 0; }

    private void refreshInputs() {
        if (binds == null) return;
        int held = stateChord(keyMask | axisMask | hatMask);
        long r = Native.bindUpdate(held, android.os.SystemClock.uptimeMillis());
        int swallow = (int) r;
        keys = Native.bindGameMask(held & ~swallow);
        Native.setPad(keys | dbg);
        if (held != lastHeld || (int) r != lastSwallow) { android.util.Log.i("somkeys", String.format("held %08x swallow %08x event %d game %03x", held, (int) r, (int) (r >> 32), keys)); lastHeld = held; lastSwallow = (int) r; }
        binds.saveIfDirty();
        info.removeCallbacks(tick);
        if (held != 0) info.postDelayed(tick, 30);   /* a held button needs a clock */
    }

    /* Pad panel mode: a click of the left stick (L3) hands the pad to the bottom panel: D-pad / stick move a focus ring over its buttons, A presses
     * the focused one, B goes back; L3 again returns the pad to the game.  Nothing reaches the game meanwhile. */
    private boolean padPanel;
    private int navHatX, navHatY;

    private void setPadPanel(boolean on) {
        padPanel = on;
        keyMask = axisMask = hatMask = 0; stickL = stickR = 0; ltDown = rtDown = false;
        navHatX = navHatY = 0;
        refreshInputs();
        Native.navRequest(7);
        android.util.Log.i("somkeys", "pad panel mode " + on);
    }

    private boolean navKey(KeyEvent e) {
        if (e.getAction() != KeyEvent.ACTION_DOWN || e.getRepeatCount() != 0) return true;
        switch (e.getKeyCode()) {
            case KeyEvent.KEYCODE_DPAD_UP: Native.navRequest(1); break;
            case KeyEvent.KEYCODE_DPAD_DOWN: Native.navRequest(2); break;
            case KeyEvent.KEYCODE_DPAD_LEFT: Native.navRequest(3); break;
            case KeyEvent.KEYCODE_DPAD_RIGHT: Native.navRequest(4); break;
            case KeyEvent.KEYCODE_BUTTON_A: case KeyEvent.KEYCODE_DPAD_CENTER: Native.navRequest(5); break;
            case KeyEvent.KEYCODE_BUTTON_B: Native.navRequest(6); break;
            default: break;
        }
        return true;
    }

    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        int code = e.getKeyCode();
        if (code == KeyEvent.KEYCODE_BUTTON_THUMBL && e.getAction() != KeyEvent.ACTION_MULTIPLE) {
            if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0) setPadPanel(!padPanel);
            return true;
        }
        if (!padPanel && gamepadKey(code) && settings != null && settings.fastForwardKey(e)) return true;
        if (padPanel && gamepadKey(code) && e.getAction() != KeyEvent.ACTION_MULTIPLE) return navKey(e);
        if (!gamepadKey(code) || e.getAction() == KeyEvent.ACTION_MULTIPLE) return super.dispatchKeyEvent(e);
        int in = Native.bindKeyInput(code);
        android.util.Log.i("somraw", KeyEvent.keyCodeToString(code) + " action " + e.getAction() + " repeat " + e.getRepeatCount() + " dev " + e.getDeviceId());
        if (e.getAction() == KeyEvent.ACTION_DOWN && android.os.SystemClock.uptimeMillis() < inputQuietUntil) {
            android.util.Log.i("somkeys", "ignored early press " + KeyEvent.keyCodeToString(code));
            return true;
        }
        if (in != 0 && e.getRepeatCount() == 0) {
            if (e.getAction() == KeyEvent.ACTION_DOWN) keyMask |= bitOf(in); else keyMask &= ~bitOf(in);
            android.util.Log.i("somkeys", KeyEvent.keyCodeToString(code) + (e.getAction() == KeyEvent.ACTION_DOWN ? " down" : " up") + " input " + in);
            refreshInputs();
        }
        return true;   /* pad buttons never reach the focus system (no stray clicks) */
    }

    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) {
        if ((e.getSource() & InputDevice.SOURCE_JOYSTICK) == 0) return super.dispatchGenericMotionEvent(e);
        if (padPanel) {                                       /* hat and left stick edges become focus moves */
            for (int h = 0; h <= e.getHistorySize(); h++) {
                boolean cur = h == e.getHistorySize();
                float x = cur ? e.getAxisValue(MotionEvent.AXIS_HAT_X) : e.getHistoricalAxisValue(MotionEvent.AXIS_HAT_X, h);
                float y = cur ? e.getAxisValue(MotionEvent.AXIS_HAT_Y) : e.getHistoricalAxisValue(MotionEvent.AXIS_HAT_Y, h);
                if (x == 0 && y == 0) {
                    x = cur ? e.getAxisValue(MotionEvent.AXIS_X) : e.getHistoricalAxisValue(MotionEvent.AXIS_X, h);
                    y = cur ? e.getAxisValue(MotionEvent.AXIS_Y) : e.getHistoricalAxisValue(MotionEvent.AXIS_Y, h);
                }
                int nx = x < -0.6f ? -1 : x > 0.6f ? 1 : 0, ny = y < -0.6f ? -1 : y > 0.6f ? 1 : 0;
                if (nx != navHatX && nx != 0) Native.navRequest(nx < 0 ? 3 : 4);
                if (ny != navHatY && ny != 0) Native.navRequest(ny < 0 ? 1 : 2);
                navHatX = nx; navHatY = ny;
            }
            return true;
        }
        float hx = e.getAxisValue(MotionEvent.AXIS_HAT_X), hy = e.getAxisValue(MotionEvent.AXIS_HAT_Y);
        int hat = 0;
        if (hy < -0.5f) hat |= bitOf(16);
        if (hy > 0.5f) hat |= bitOf(17);
        if (hx < -0.5f) hat |= bitOf(18);
        if (hx > 0.5f) hat |= bitOf(19);
        hatMask = hat;
        if (android.os.SystemClock.uptimeMillis() < inputQuietUntil) { hatMask = 0; return true; }
        stickL = Native.bindStick(stickL, e.getAxisValue(MotionEvent.AXIS_X), e.getAxisValue(MotionEvent.AXIS_Y));
        float rx = e.getAxisValue(MotionEvent.AXIS_Z), ry = e.getAxisValue(MotionEvent.AXIS_RZ);
        if (Math.abs(e.getAxisValue(MotionEvent.AXIS_RX)) > Math.abs(rx)) rx = e.getAxisValue(MotionEvent.AXIS_RX);
        if (Math.abs(e.getAxisValue(MotionEvent.AXIS_RY)) > Math.abs(ry)) ry = e.getAxisValue(MotionEvent.AXIS_RY);
        stickR = Native.bindStick(stickR, rx, ry);
        ltDown = Native.bindTrigger(ltDown, Math.max(e.getAxisValue(MotionEvent.AXIS_LTRIGGER), e.getAxisValue(MotionEvent.AXIS_BRAKE)));
        rtDown = Native.bindTrigger(rtDown, Math.max(e.getAxisValue(MotionEvent.AXIS_RTRIGGER), e.getAxisValue(MotionEvent.AXIS_GAS)));
        int m = 0;
        for (int i = 0; i < 4; i++) {
            if ((stickL >> i & 1) != 0) m |= bitOf(20 + i);   /* left stick up, down, left, right = ids 20..23 */
            if ((stickR >> i & 1) != 0) m |= bitOf(24 + i);   /* right stick = ids 24..27 */
        }
        if (ltDown) m |= bitOf(7);
        if (rtDown) m |= bitOf(8);
        axisMask = m;
        refreshInputs();
        return true;
    }
}
