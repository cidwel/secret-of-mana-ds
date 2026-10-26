package org.somdecomp.companion;

import android.content.Context;
import android.content.SharedPreferences;

/** Persistence of the quick slots (the four shoulder slots L1 R1 L2 R2 are slots 0..3) and of the discovered weapons, spirits and items.
 *  Round 3: both belong to one game, not to the ROM variant: the key is the hash of the three hero names (Native.gameKey), so a new game starts
 *  with empty slots and an empty discovered set, and a loaded save gets back what was stored for its names.  At the title nothing is active.
 *  Limit: two saves with identical hero names share their entry (the game exposes no save-slot number in WRAM). */
final class Bindings {
    private final SharedPreferences sp;
    private String variant = "none";
    private String key = "";
    private int savedSeen;
    private static Bindings inst;

    static Bindings instance(Context c) { if (inst == null) inst = new Bindings(c.getApplicationContext()); return inst; }

    private Bindings(Context c) { sp = c.getSharedPreferences("som", Context.MODE_PRIVATE); }

    private String pk(String what) { return what(what, key); }
    private String what(String what, String k) { return what + "_" + variant + "_" + k; }

    void load() {
        variant = Native.romVariant();
        Native.bindLoad(null);
        if (sp.contains("binds2_" + variant)) sp.edit().remove("binds2_" + variant).apply();   /* old bindings: migrated away */
        key = "";
        savedSeen = 0;
        Native.quickClear();   /* the pre-round-3 per-variant slots (prefs key quick_<variant>) are deliberately not read: they were the stale ones */
        for (int h = 0; h < 3; h++) Native.setLastName(h, sp.getString("name_last_" + h, ""));
    }

    /** LAST NAME key: the last name confirmed for each hero (boy, girl, sprite), in any game and ROM variant. */
    private void saveNames() {
        String n = Native.nameConfirmed();
        if (n.length() < 3 || n.charAt(1) != ' ') return;
        int h = n.charAt(0) - '0';
        String name = n.substring(2);
        if (h < 0 || h > 2 || name.isEmpty()) return;
        sp.edit().putString("name_last_" + h, name).apply();
        Native.setLastName(h, name);
    }

    /** Called about every 60 ms: follows the game identity and stores what changed. */
    void saveIfDirty() {
        saveNames();
        int ev = Native.gameEvent();
        String k = Native.gameKey();
        if (k.isEmpty()) {                                   /* title, game select, name entry: no game */
            if (!key.isEmpty()) { key = ""; savedSeen = 0; Native.quickClear(); }
            return;
        }
        if (ev == 1) {                                       /* a new game began: whatever was stored for these names is from an older game */
            key = k;
            sp.edit().remove(pk("quick")).remove(pk("seen")).apply();
            Native.quickClear();
            savedSeen = 0;
        } else if (ev == 3 && !key.isEmpty() && !k.equals(key)) {   /* a hero joined and was named: same game, its data moves to the new key */
            String quick = Native.quickSave();
            sp.edit().putString(what("quick", k), quick).putInt(what("seen", k), Native.seenGet()).apply();   /* the old key stays for saves made before the join */
            key = k;
            savedSeen = Native.seenGet();
        } else if (!k.equals(key)) {                         /* a save was loaded (or a state): its own slots and discoveries */
            key = k;
            Native.quickLoad(sp.getString(pk("quick"), null));
            savedSeen = sp.getInt(pk("seen"), 0);
            Native.seenSet(savedSeen);
        }
        SharedPreferences.Editor e = null;
        if (Native.quickDirty()) { e = sp.edit(); e.putString(pk("quick"), Native.quickSave()); }
        int seen = Native.seenGet();
        if (seen != savedSeen) { if (e == null) e = sp.edit(); e.putInt(pk("seen"), seen); savedSeen = seen; }
        if (e != null) e.apply();
    }
}
