package org.somdecomp.companion;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;

/**
 * Menu sounds.  The game's own effects need the emulator core (they are played through its sound chip), which is not running in the menu,
 * so these are short synthesised blips with the same roles: cursor, accept, back, error.
 */
final class MenuSound {
    static final int MOVE = 0, OK = 1, BACK = 2, ERROR = 3;
    private static final int RATE = 22050;
    private final AudioTrack[] t = new AudioTrack[4];

    MenuSound() {
        t[MOVE] = make(new int[]{1400}, new int[]{22}, 0.35);
        t[OK] = make(new int[]{880, 1320}, new int[]{50, 70}, 0.45);
        t[BACK] = make(new int[]{700, 520}, new int[]{45, 60}, 0.4);
        t[ERROR] = make(new int[]{170, 150}, new int[]{70, 90}, 0.5);
    }

    private static AudioTrack make(int[] hz, int[] ms, double vol) {
        int n = 0;
        for (int m : ms) n += RATE * m / 1000;
        short[] pcm = new short[n];
        int k = 0;
        for (int i = 0; i < hz.length; i++) {
            int len = RATE * ms[i] / 1000;
            for (int j = 0; j < len; j++, k++) {
                double ph = (double) j * hz[i] / RATE;
                double sq = (ph - Math.floor(ph)) < 0.5 ? 1 : -1;               /* square wave, like a console voice */
                double env = Math.min(1.0, (len - j) / (len * 0.4)) * Math.min(1.0, j / 40.0);
                pcm[k] = (short) (sq * env * vol * 9000);
            }
        }
        try {
            AudioTrack a = new AudioTrack.Builder()
                    .setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build())
                    .setAudioFormat(new AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT).setSampleRate(RATE).setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build())
                    .setTransferMode(AudioTrack.MODE_STATIC).setBufferSizeInBytes(pcm.length * 2).build();
            a.write(pcm, 0, pcm.length);
            return a;
        } catch (RuntimeException e) { return null; }
    }

    void play(int which) {
        android.util.Log.i("somcompanion", "menu sfx " + which);
        AudioTrack a = t[which];
        if (a == null) return;
        try { a.stop(); a.reloadStaticData(); a.play(); } catch (RuntimeException ignored) { /* no audio */ }
    }

    void release() { for (AudioTrack a : t) if (a != null) a.release(); }
}
