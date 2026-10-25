package org.somdecomp.companion;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.KeyEvent;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import java.io.File;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Start menu: "SoM Original" or "Create Randomized".  The model, the drawing and the configuration live in C (tools/companion/launcher).
 * ROMs live in the app files directory (never in the APK): rom.sfc is the user's US v1.0 ROM, roms/gen/<key>.sfc are generated ROMs.
 * Generation goes through the libsomrando seam (LauncherNative.generatorAvailable / nativeGenerate).  Without it, a ROM for the chosen
 * configuration that the PC pushed with "som-dualscreen push-random" is played; otherwise the menu says the generator is not available.
 */
public final class StartMenuActivity extends Activity {
    /** Shown when the device cannot make this game itself.  It names no library: the player only needs to know what to do instead. */
    static final String UNAVAILABLE = "This device cannot create new randomized games yet. Create the game on your PC (som-dualscreen push-random) and it will show up in Saved Randomized.";
    private LauncherView view, view2;
    private android.app.Presentation panel;
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final AtomicBoolean cancel = new AtomicBoolean();
    private Thread worker;
    static StartMenuActivity current;   /* the second-screen view routes its touches through it */
    private MenuSound sound;
    private int lastScreen = -1;
    private boolean dual;   /* a second display is present: this activity's screen shows the wallpaper / summary, the menu is on the panel */

    private int supportRev = -1;

    private final Runnable loop = new Runnable() {
        @Override public void run() {
            LauncherNative.tick();
            int rv = LauncherNative.revision();
            if (rv != supportRev) {   /* the dim GO / "PC only" labels follow the configuration */
                try { LauncherNative.refreshConfigSupport(); } catch (Throwable ignored) { /* no generator */ }
                supportRev = LauncherNative.revision();
            }
            savePagesIfDirty();
            int act;
            while ((act = LauncherNative.poll()) != LauncherNative.ACT_NONE) onAction(act);
            if (view != null) view.refresh();
            if (view2 != null) view2.refresh();
            ui.postDelayed(this, 33);
        }
    };

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        current = this;
        sound = new MenuSound();
        LauncherNative.init(System.nanoTime() ^ (System.currentTimeMillis() << 20));
        try (InputStream pk = getAssets().open("som_ui_pack.bin")) { LauncherNative.loadPack(readAll(pk)); } catch (Exception ignored) { /* built-in face */ }
        try { LauncherNative.refreshSupport(); } catch (Throwable ignored) { /* no generator */ }
        initPages();
        dual = hasPresentationDisplay();
        refreshSaved();
        refreshStatus();
        view = new LauncherView(this, dual);
        setContentView(view);
        WindowInsetsController wic = getWindow().getInsetsController();
        if (wic != null) {
            wic.hide(WindowInsets.Type.systemBars());
            wic.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        }
        if (getIntent().getBooleanExtra("auto_original", false)) ui.post(() -> onAction(LauncherNative.ACT_PLAY_ORIGINAL));
    }

    @Override protected void onResume() { super.onResume(); refreshSaved(); refreshStatus(); showPanel(); ui.post(loop); }
    @Override protected void onPause() { ui.removeCallbacks(loop); super.onPause(); }
    @Override protected void onStop() { hidePanel(); super.onStop(); }
    @Override protected void onDestroy() { if (sound != null) sound.release(); if (current == this) current = null; super.onDestroy(); }

    private boolean hasPresentationDisplay() {
        android.hardware.display.DisplayManager dm = (android.hardware.display.DisplayManager) getSystemService(DISPLAY_SERVICE);
        return dm.getDisplays(android.hardware.display.DisplayManager.DISPLAY_CATEGORY_PRESENTATION).length > 0;
    }

    /** The second screen (AYN Thor bottom panel) carries the menu so it can be driven by touch; the main screen shows the wallpaper and the summary. */
    private void showPanel() {
        if (panel != null) return;
        android.hardware.display.DisplayManager dm = (android.hardware.display.DisplayManager) getSystemService(DISPLAY_SERVICE);
        android.view.Display[] ds = dm.getDisplays(android.hardware.display.DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
        if (ds.length == 0) return;
        panel = new android.app.Presentation(this, ds[0]) {
            @Override protected void onCreate(Bundle b) {
                super.onCreate(b);
                view2 = new LauncherView(getContext());
                android.widget.FrameLayout f = new android.widget.FrameLayout(getContext());
                f.setBackgroundColor(0xFF000000);
                f.setFitsSystemWindows(true);   /* keep the menu above the navigation bar of the second screen (1240x969) */
                f.addView(view2, new android.widget.FrameLayout.LayoutParams(-1, -1));
                setContentView(f);
            }
        };
        try { panel.show(); } catch (RuntimeException e) {
            panel = null; view2 = null;
            if (dual) { dual = false; view = new LauncherView(this, false); setContentView(view); }   /* no panel: the main screen carries the menu */
        }
    }

    private void hidePanel() {
        if (panel != null) { try { panel.dismiss(); } catch (RuntimeException ignored) { /* gone */ } panel = null; view2 = null; }
    }

    private File filesDir() { File d = getExternalFilesDir(null); return d != null ? d : getFilesDir(); }
    private File originalRom() { return new File(filesDir(), "rom.sfc"); }
    private File genDir() { return new File(filesDir(), "roms/gen"); }

    private File trashDir() { return new File(filesDir(), "roms/trash"); }

    /** Lists the pre-generated ROMs (roms/gen/<key>.sfc with its sidecar), newest first, four at most, for the Saved Randomized screen. */
    private void refreshSaved() {
        try {
            LauncherNative.savedClear();
            File[] f = genDir().listFiles((d, n) -> n.endsWith(".sfc"));
            if (f == null) return;
            java.util.ArrayList<File> ok = new java.util.ArrayList<>();
            for (File x : f) if (looksGenerated(x)) ok.add(x);
            ok.sort((a, b) -> Long.compare(b.lastModified(), a.lastModified()));
            int n = 0;
            for (File x : ok) { if (n >= 4) break; addSaved(x); n++; }
            LauncherNative.savedHidden(ok.size() - n);
        } catch (Throwable t) { android.util.Log.w("somcompanion", "saved list failed: " + t); }
    }

    private void addSaved(File sfc) {
        String name = sfc.getName(), key = name.substring(0, name.length() - 4);
        String preset = "", mode = "", seed = "", diff = "";
        StringBuilder lines = new StringBuilder();
        try {
            byte[] raw = java.nio.file.Files.readAllBytes(new File(genDir(), key + ".json").toPath());
            org.json.JSONObject o = new org.json.JSONObject(new String(raw, "UTF-8"));
            org.json.JSONObject c = o.optJSONObject("config");
            if (c == null) c = o;
            preset = c.optString("preset", ""); mode = c.optString("mode", o.optString("mode", ""));
            seed = c.optString("seed", o.optString("seed", "")); diff = c.optString("difficulty", o.optString("difficulty", ""));
            org.json.JSONObject opts = c.optJSONObject("options");
            if (opts != null) for (java.util.Iterator<String> it = opts.keys(); it.hasNext();) { String k = it.next(); lines.append(k).append('=').append(opts.getString(k)).append('\n'); }
        } catch (Exception ignored) { /* no sidecar: the slot shows the key */ }
        if (mode.isEmpty()) { preset = "Game " + key.substring(0, Math.min(8, key.length())); seed = "?"; }
        String date = new java.text.SimpleDateFormat("yyyy-MM-dd", java.util.Locale.US).format(new java.util.Date(sfc.lastModified()));
        LauncherNative.savedAdd(key, preset, mode, seed, diff, date, sfc.getPath(), lines.toString());
    }

    /** Moves a saved game to roms/trash (never deletes), so a mistaken delete can be undone by moving the files back. */
    private void trashSaved(String key) {
        File t = trashDir();
        t.mkdirs();
        String stamp = new java.text.SimpleDateFormat("yyyyMMdd-HHmmss", java.util.Locale.US).format(new java.util.Date());
        for (String ext : new String[] {".sfc", ".json"}) {
            File f = new File(genDir(), key + ext);
            if (f.exists() && !f.renameTo(new File(t, stamp + "-" + key + ext))) android.util.Log.w("somcompanion", "could not move " + f);
        }
    }

    /** Four saved games at most: before a new one is added the oldest ones go to the trash. */
    private void makeRoomFor(String key) {
        File[] f = genDir().listFiles((d, n) -> n.endsWith(".sfc") && !n.equals(key + ".sfc"));
        if (f == null) return;
        java.util.ArrayList<File> ok = new java.util.ArrayList<>();
        for (File x : f) if (looksGenerated(x)) ok.add(x);
        ok.sort((a, b) -> Long.compare(a.lastModified(), b.lastModified()));
        for (int i = 0; ok.size() - i >= 4; i++) { String n = ok.get(i).getName(); trashSaved(n.substring(0, n.length() - 4)); }
    }

    private void refreshStatus() {
        String orig = originalRom().exists() ? "" : "Choose your ROM after start";
        File[] gen = genDir().listFiles((d, n) -> n.endsWith(".sfc"));
        int ok = 0, total = 0;
        try { for (int p = 0; p < 64; p++) { int r = LauncherNative.presetSupported(p); if (r < 0) break; total++; ok += r > 0 ? 1 : 0; } } catch (Throwable ignored) { total = 0; }
        String g = ok > 0 ? ""
                : (gen != null && gen.length > 0 ? "Pre-generated games only" : "Needs a game made on the PC");
        LauncherNative.setStatus(orig, g);
    }

    /** True when the on-device generator is linked and supports the menu's current configuration.  A missing or broken native generator means false, never a crash. */
    private static boolean generatorReady() {
        try { return LauncherNative.generatorAvailable(); } catch (Throwable t) { return false; }
    }

    private static String generatorWhy() {
        try { return LauncherNative.generatorWhy(); } catch (Throwable t) { return ""; }
    }

    /** The ROM must be 4 MiB with the Secret of MANA header title; a sidecar is not required. */
    static boolean looksGenerated(File f) {
        if (!f.isFile() || f.length() != 4L * 1024 * 1024) return false;
        try (RandomAccessFile r = new RandomAccessFile(f, "r")) {
            byte[] t = new byte[14];
            r.seek(0xFFC0);
            r.readFully(t);
            return "Secret of MANA".equals(new String(t, "ISO-8859-1"));
        } catch (Exception e) { return false; }
    }

    /** Runs an input and plays the sound that fits what it did: cursor move, accept (screen forward or value change), back, error. */
    void input(boolean back, Runnable r) {
        int rev0 = LauncherNative.revision(), a = LauncherNative.snapshot();
        r.run();
        int b = LauncherNative.snapshot(), rev1 = LauncherNative.revision();
        android.util.Log.i("somcompanion", "menu input screen " + (a & 0xFF) + ">" + (b & 0xFF) + " cursor " + (a >> 8 & 0xFFFF) + ">" + (b >> 8 & 0xFFFF));
        if (sound == null || rev0 == rev1) return;
        if ((b & 0xFF) != (a & 0xFF)) sound.play((b & 0xFF) < (a & 0xFF) || back ? MenuSound.BACK : MenuSound.OK);
        else if ((b >> 30 & 1) != 0 && (a >> 30 & 1) == 0) sound.play(MenuSound.ERROR);
        else if ((b & 0xFFFFFF00) != (a & 0xFFFFFF00) && ((b >> 8 & 0xFFFF) != (a >> 8 & 0xFFFF) || (b >> 24 & 0x3F) != (a >> 24 & 0x3F))) sound.play(MenuSound.MOVE);
        else sound.play(MenuSound.OK);
    }

    private void onAction(int act) {
        switch (act) {
            case LauncherNative.ACT_PLAY_ORIGINAL:
                startGame(null);
                break;
            case LauncherNative.ACT_GENERATE:
                generate();
                break;
            case LauncherNative.ACT_CANCEL:
                cancel.set(true);
                break;
            case LauncherNative.ACT_PLAY:
                startGame(LauncherNative.romPath());
                break;
            case LauncherNative.ACT_QUIT:
                finish();
                break;
            case LauncherNative.ACT_DELETE_SAVED: {
                String key = LauncherNative.savedKey(LauncherNative.actArg());
                if (!key.isEmpty()) { trashSaved(key); refreshSaved(); refreshStatus(); }
                break;
            }
            default: break;
        }
    }

    /* ---- gameplay tweaks and credits pages (X / Y on the main screen) ---- */
    private File gameplayValues() { return new File(getFilesDir(), "gameplay.json"); }

    private String readAsset(String name) {
        try (InputStream in = getAssets().open(name)) { return new String(readAll(in), "UTF-8"); } catch (Exception e) { return ""; }
    }

    private void initPages() {
        try {
            String values = "";
            try { values = new String(java.nio.file.Files.readAllBytes(gameplayValues().toPath()), "UTF-8"); } catch (Exception ignored) { /* none saved yet */ }
            LauncherNative.pagesInit(readAsset("gameplay_menu.json"), values, readAsset("credits.json"));
        } catch (Throwable t) { android.util.Log.w("somcompanion", "pages unavailable: " + t); }
    }

    private void savePagesIfDirty() {
        try {
            if (!LauncherNative.gameplayDirty()) return;
            File f = gameplayValues(), tmp = new File(f.getPath() + ".part");
            java.nio.file.Files.write(tmp.toPath(), LauncherNative.gameplayJson().getBytes("UTF-8"));
            if (!tmp.renameTo(f)) android.util.Log.w("somcompanion", "could not save gameplay.json");
        } catch (Throwable t) { android.util.Log.w("somcompanion", "gameplay save failed: " + t); }
    }

    private void startGame(String romPath) {
        Intent i = new Intent(this, MainActivity.class);
        if (getIntent().getExtras() != null) i.putExtras(getIntent().getExtras());   /* adb test extras (ws, fit, hide_hud) reach the game */
        if (romPath != null) i.putExtra("rom_path", romPath).putExtra("rom_key", LauncherNative.configKey());
        String sha = "";
        File src = romPath != null ? new File(romPath) : originalRom();
        String base = null, sig = "";
        if (romPath != null) base = RunKeys.generatedBase(LauncherNative.configKey());
        else if (src.exists()) { sha = sha256(src); base = RunKeys.originalBase(sha); }
        /* Gameplay tweaks go into a patched copy; the user's ROM file is never written. */
        if (base != null) {
            try {
                savePagesIfDirty();
                if (!LauncherNative.gameplayIsDefault()) {
                    String opts = LauncherNative.gameplayJson();
                    sig = RunKeys.signature(opts);
                    if (!sig.isEmpty()) {
                        File out = new File(getFilesDir(), "roms/gameplay/" + base + "-" + sig + ".sfc");
                        out.getParentFile().mkdirs();
                        String[] skipped = new String[1];
                        String err = LauncherNative.gameplayApply(src.getPath(), opts, out.getPath(), skipped);
                        if (!err.isEmpty()) { LauncherNative.genFail("Gameplay tweaks could not be applied: " + err); return; }
                        i.putExtra("rom_path", out.getPath());
                        if (skipped[0] != null && !skipped[0].isEmpty()) i.putExtra("gp_skipped", skipped[0]);
                    }
                }
            } catch (Throwable t) { android.util.Log.w("somcompanion", "gameplay apply unavailable: " + t); sig = ""; }
            i.putExtra("base_key", base).putExtra("gp_sig", sig);
        }
        startActivity(i);
        finish();
    }

    /** Lower-case hex SHA-256 of a file, "" when it cannot be read. */
    static String sha256(File f) {
        try (InputStream in = new java.io.FileInputStream(f)) {
            java.security.MessageDigest md = java.security.MessageDigest.getInstance("SHA-256");
            byte[] buf = new byte[1 << 16];
            for (int n; (n = in.read(buf)) > 0; ) md.update(buf, 0, n);
            StringBuilder sb = new StringBuilder();
            for (byte b : md.digest()) sb.append(String.format("%02x", b));
            return sb.toString();
        } catch (Exception e) { return ""; }
    }

    private void generate() {
        String key = LauncherNative.configKey();
        File out = new File(genDir(), key + ".sfc");
        if (looksGenerated(out)) { out.setLastModified(System.currentTimeMillis()); LauncherNative.genCached(out.getPath()); return; }
        if (!generatorReady()) {
            String why = generatorWhy();
            LauncherNative.genUnavailable(why.isEmpty() || why.startsWith("generator not advertised") ? UNAVAILABLE
                    : "This setup is not supported by the on-device generator yet: " + why + ". Create it on your PC (som-dualscreen push-random).");
            return;
        }
        File src = originalRom();
        if (!src.exists()) { LauncherNative.genFail("No US Secret of Mana ROM found. Push it as rom.sfc first."); return; }
        genDir().mkdirs();
        final String mode = LauncherNative.configField(0), seed = LauncherNative.configField(1), diff = LauncherNative.configField(2);
        final String lines = LauncherNative.configLines(), cfgJson = LauncherNative.configJson();
        final File part = new File(genDir(), key + ".sfc.part");
        cancel.set(false);
        LauncherNative.genBegin();
        worker = new Thread(() -> {
            String[] err = new String[1];
            int rc;
            try {
                rc = LauncherNative.nativeGenerate(mode, seed, diff.isEmpty() ? null : diff, lines, src.getPath(), part.getPath(),
                        getCacheDir().getPath(), (step, total, name) -> {
                            ui.post(() -> LauncherNative.genProgress(step, total, name));
                            return cancel.get();
                        }, err);
            } catch (Throwable t) {   /* the generator could not even be called: report it as unavailable */
                rc = LauncherNative.RANDO_UNAVAILABLE; err[0] = UNAVAILABLE;
            }
            final int code = rc;
            final String sha = code == LauncherNative.RANDO_OK ? sha256(part) : "";   /* device-side self-report: compare with `som-dualscreen generate` on the PC */
            ui.post(() -> {
                if (code == LauncherNative.RANDO_OK && looksGenerated(part)) {
                    makeRoomFor(key);
                    if (part.renameTo(out)) {
                        android.util.Log.i("somcompanion", "generated key=" + key + " mode=" + mode + " seed=" + seed + " sha256=" + sha);
                        try { java.nio.file.Files.write(new File(genDir(), key + ".json").toPath(), ("{\"tool\":\"companion\",\"created\":" + System.currentTimeMillis() + ",\"sha256\":\"" + sha + "\",\"config\":" + cfgJson + "}").getBytes("UTF-8")); } catch (Exception ignored) { /* the slot then shows the key only */ }
                        refreshSaved();
                        LauncherNative.genDone(out.getPath());
                    } else LauncherNative.genFail("Could not save the new game");
                }
                else if (code == LauncherNative.RANDO_CANCELLED) { /* the menu already went back */ }
                else if (code == LauncherNative.RANDO_UNAVAILABLE) LauncherNative.genUnavailable(UNAVAILABLE);
                else LauncherNative.genFail(err[0] != null && !err[0].isEmpty() ? err[0] : "Generation failed");
                if (code != LauncherNative.RANDO_OK && part.exists()) part.renameTo(new File(part.getPath() + ".failed"));
            });
        }, "somrando");
        worker.start();
    }

    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        if (e.getAction() == KeyEvent.ACTION_DOWN) {
            int b = -1;
            switch (e.getKeyCode()) {
                case KeyEvent.KEYCODE_DPAD_UP: b = LauncherNative.B_UP; break;
                case KeyEvent.KEYCODE_DPAD_DOWN: b = LauncherNative.B_DOWN; break;
                case KeyEvent.KEYCODE_DPAD_LEFT: b = LauncherNative.B_LEFT; break;
                case KeyEvent.KEYCODE_DPAD_RIGHT: b = LauncherNative.B_RIGHT; break;
                case KeyEvent.KEYCODE_BUTTON_A: case KeyEvent.KEYCODE_ENTER: case KeyEvent.KEYCODE_DPAD_CENTER: b = LauncherNative.B_A; break;
                case KeyEvent.KEYCODE_BUTTON_B: case KeyEvent.KEYCODE_BACK: case KeyEvent.KEYCODE_ESCAPE: b = LauncherNative.B_B; break;
                case KeyEvent.KEYCODE_BUTTON_X: b = LauncherNative.B_X; break;
                case KeyEvent.KEYCODE_BUTTON_Y: b = LauncherNative.B_Y; break;
                case KeyEvent.KEYCODE_BUTTON_L1: b = LauncherNative.B_L; break;
                case KeyEvent.KEYCODE_BUTTON_R1: b = LauncherNative.B_R; break;
                case KeyEvent.KEYCODE_BUTTON_START: b = LauncherNative.B_START; break;
                case KeyEvent.KEYCODE_BUTTON_SELECT: b = LauncherNative.B_SELECT; break;
                default: break;
            }
            if (b >= 0) { final int bb = b; input(b == LauncherNative.B_B, () -> LauncherNative.key(bb)); return true; }
        } else if (e.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            return true;
        }
        return super.dispatchKeyEvent(e);
    }

    private int hatX, hatY;   /* last D-pad (hat) direction: a press is the edge from 0 to +-1 */

    /** The Thor's D-pad and left stick arrive as hat/stick motion, not as DPAD key codes: turn each new direction into one key. */
    @Override public boolean dispatchGenericMotionEvent(android.view.MotionEvent e) {
        if ((e.getSource() & android.view.InputDevice.SOURCE_JOYSTICK) != 0 && e.getActionMasked() == android.view.MotionEvent.ACTION_MOVE) {
            for (int h = 0; h <= e.getHistorySize(); h++) {   /* a quick press and release can arrive batched: look at every sample */
                boolean cur = h == e.getHistorySize();
                float x = cur ? e.getAxisValue(android.view.MotionEvent.AXIS_HAT_X) : e.getHistoricalAxisValue(android.view.MotionEvent.AXIS_HAT_X, h);
                float y = cur ? e.getAxisValue(android.view.MotionEvent.AXIS_HAT_Y) : e.getHistoricalAxisValue(android.view.MotionEvent.AXIS_HAT_Y, h);
                if (x == 0 && y == 0) {
                    x = cur ? e.getAxisValue(android.view.MotionEvent.AXIS_X) : e.getHistoricalAxisValue(android.view.MotionEvent.AXIS_X, h);
                    y = cur ? e.getAxisValue(android.view.MotionEvent.AXIS_Y) : e.getHistoricalAxisValue(android.view.MotionEvent.AXIS_Y, h);
                }
                int nx = x < -0.6f ? -1 : x > 0.6f ? 1 : 0, ny = y < -0.6f ? -1 : y > 0.6f ? 1 : 0;
                if (nx != hatX && nx != 0) { final int k = nx < 0 ? LauncherNative.B_LEFT : LauncherNative.B_RIGHT; input(false, () -> LauncherNative.key(k)); }
                if (ny != hatY && ny != 0) { final int k = ny < 0 ? LauncherNative.B_UP : LauncherNative.B_DOWN; input(false, () -> LauncherNative.key(k)); }
                hatX = nx; hatY = ny;
            }
            return true;
        }
        return super.dispatchGenericMotionEvent(e);
    }

    private static byte[] readAll(InputStream in) throws java.io.IOException {
        java.io.ByteArrayOutputStream o = new java.io.ByteArrayOutputStream();
        byte[] buf = new byte[65536];
        int n;
        while ((n = in.read(buf)) > 0) o.write(buf, 0, n);
        return o.toByteArray();
    }
}
