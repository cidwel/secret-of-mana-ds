package org.somdecomp.companion;

import java.security.MessageDigest;

/**
 * Identity of a run for saves and save states: the base key names the game (the original ROM or one generated configuration), the signature names
 * the gameplay tweaks applied to it.  The cartridge RAM is kept per base key (so toggling a tweak never forks the save); save states also carry the
 * signature (a state made under other tweaks would run other code).  Pure Java, tested by android/tests/jvm.
 */
final class RunKeys {
    private RunKeys() { }

    static String hex(byte[] d, int nBytes) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < nBytes && i < d.length; i++) sb.append(String.format("%02x", d[i] & 255));
        return sb.toString();
    }

    static byte[] sha256(byte[] data) {
        try { return MessageDigest.getInstance("SHA-256").digest(data); }
        catch (java.security.NoSuchAlgorithmException e) { throw new IllegalStateException(e); }
    }

    /** Base key of the original ROM: "orig-" and the first 12 hex digits of the ROM's SHA-256. */
    static String originalBase(String romSha256) { return "orig-" + (romSha256.length() > 12 ? romSha256.substring(0, 12) : romSha256); }

    /** Base key of a generated game: the launcher's configuration key (already file-name safe). */
    static String generatedBase(String configKey) { return "gen-" + configKey.replaceAll("[^0-9A-Za-z_-]", "_"); }

    /** Signature of a gameplay options document as written by the native page (canonical: a fixed key order, no spaces): "" for none or only defaults,
     *  else 8 hex digits of the SHA-256 of the text.  Whitespace is ignored, so a hand-edited file with the same content keeps its states. */
    static String signature(String optionsJson) {
        if (optionsJson == null) return "";
        String c = optionsJson.replaceAll("\\s+", "");
        if (c.isEmpty() || c.equals("{}") || c.equals("{\"options\":{}}")) return "";
        return hex(sha256(c.getBytes(java.nio.charset.StandardCharsets.UTF_8)), 4);
    }
}
