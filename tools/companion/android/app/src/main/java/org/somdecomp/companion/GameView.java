package org.somdecomp.companion;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.RuntimeShader;
import android.graphics.Shader;
import android.os.Build;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.View;

/** Top screen: the game frame (always the whole frame: constant geometry), fit with 8:7 pixels, nearest-neighbour, or one of the retro filters (Settings.shader). */
final class GameView extends View {
    private Bitmap bmp;
    private final Paint paint = new Paint();
    private final RectF dst = new RectF();
    private static final float REFH = 224f;

    /* Filters (round 4).  Every program gets `scale` (screen pixels per game pixel, per axis) and `dstSize`, so it behaves the same at the fractional Thor
     * scale (5.5 x 4.8), at INTEGER, at FILL / FIT / STRETCH and for every frame width of the wide cores.  AGSL needs API 33; older devices keep nearest. */
    static final int NONE = 0, SHARP = 1, SCANLINES = 2, LCD = 3, CRT = 4;
    private static final String PRELUDE = "uniform shader img; uniform float2 scale; uniform float2 dstSize;\n"
            + "float2 sharpUV(float2 p) { float2 pf = floor(p); float2 cd = p - pf - 0.5; float2 rr = max(float2(0.0), 0.5 - 0.5 / scale);\n"
            + "  return pf + 0.5 + (cd - clamp(cd, -rr, rr)) * scale; }\n"
            + "float depthOf(float s) { return clamp((s - 1.5) / 2.0, 0.0, 1.0); }\n";
    /* Sharp bilinear: every texel stays a flat block, only the one-screen-pixel seam between two texels is blended, so a fractional scale costs no
     * uneven stroke widths and no blur. */
    private static final String SRC_SHARP = PRELUDE + "half4 main(float2 fc) { return img.eval(sharpUV(fc / scale)); }";
    /* Scanlines: a gaussian beam profile per game row (dark between rows), smooth horizontally; fades out below 3.5 px per row so it never aliases. */
    private static final String SRC_SCAN = PRELUDE
            + "half4 main(float2 fc) { float2 p = fc / scale; float2 uv = sharpUV(p); half4 c = img.eval(float2(uv.x, floor(p.y) + 0.5));\n"
            + "  float y = fract(p.y) - 0.5; float beam = exp(-(y * y) / 0.10); float k = mix(1.0, mix(0.45, 1.05, beam), depthOf(scale.y));\n"
            + "  return half4(c.rgb * half(k), 1.0); }";
    /* LCD grid: every game pixel is a lit cell with a one-screen-pixel dark gap on both axes (a handheld screen), slightly brightened to make up for the gaps. */
    private static final String SRC_LCD = PRELUDE
            + "half4 main(float2 fc) { float2 p = fc / scale; half4 c = img.eval(floor(p) + 0.5); float2 f = fract(p); float2 e = min(f, 1.0 - f);\n"
            + "  float2 m = smoothstep(float2(0.0), 1.5 / scale, e); float d = depthOf(min(scale.x, scale.y));\n"
            + "  float k = mix(1.0, mix(0.50, 1.0, m.x * m.y) * 1.10, d); return half4(clamp(c.rgb * half(k), 0.0, 1.0), 1.0); }";
    /* CRT-lite: scanlines, an aperture-grille tint every third screen column, a mild vignette; no curvature, so the whole frame stays on screen. */
    private static final String SRC_CRT = PRELUDE
            + "half4 main(float2 fc) { float2 p = fc / scale; float2 uv = sharpUV(p); half4 c = img.eval(float2(uv.x, floor(p.y) + 0.5));\n"
            + "  float y = fract(p.y) - 0.5; float beam = exp(-(y * y) / 0.12); float k = mix(1.0, mix(0.50, 1.05, beam), depthOf(scale.y));\n"
            + "  float m3 = mod(fc.x, 3.0); float3 mask = float3(m3 < 1.0 ? 1.0 : 0.80, (m3 >= 1.0 && m3 < 2.0) ? 1.0 : 0.80, m3 >= 2.0 ? 1.0 : 0.80);\n"
            + "  mask = mix(float3(1.0), mask, depthOf(scale.x));\n"
            + "  float2 q = fc / dstSize - 0.5; float vig = 1.0 - dot(q, q) * 0.50;\n"
            + "  return half4(clamp(c.rgb * half3(float3(k) * mask * vig * 1.12), 0.0, 1.0), 1.0); }";
    private static final String[] SRC = {null, SRC_SHARP, SRC_SCAN, SRC_LCD, SRC_CRT};
    private final RuntimeShader[] progs = new RuntimeShader[SRC.length];
    private final boolean[] broken = new boolean[SRC.length];
    private final Paint shaderPaint = new Paint();
    private BitmapShader bshader;
    private Bitmap shaderBmp;

    GameView(Context c) {
        super(c);
        setBackgroundColor(0xFF000000);
        paint.setFilterBitmap(false);
    }

    /** Draws the frame through filter `mode` over `dst`; false when the device cannot (the caller then draws plain nearest-neighbour). */
    private boolean drawShader(Canvas canvas, int mode, int w, int h) {
        if (Build.VERSION.SDK_INT < 33 || mode <= 0 || mode >= SRC.length || broken[mode]) return false;
        try {
            if (shaderBmp != bmp) {
                bshader = new BitmapShader(bmp, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
                bshader.setFilterMode(BitmapShader.FILTER_MODE_LINEAR);
                shaderBmp = bmp;
                for (int i = 0; i < progs.length; i++) if (progs[i] != null) progs[i].setInputShader("img", bshader);
            }
            if (progs[mode] == null) { progs[mode] = new RuntimeShader(SRC[mode]); progs[mode].setInputShader("img", bshader); }
            float dw = dst.width(), dh = dst.height();
            progs[mode].setFloatUniform("scale", dw / w, dh / h);
            progs[mode].setFloatUniform("dstSize", dw, dh);
            shaderPaint.setShader(progs[mode]);
            canvas.save();
            canvas.translate(dst.left, dst.top);
            canvas.drawRect(0f, 0f, dw, dh, shaderPaint);
            canvas.restore();
            return true;
        } catch (RuntimeException e) {
            android.util.Log.w("somshader", "filter " + mode + " unusable: " + e);
            broken[mode] = true; progs[mode] = null;
            return false;
        }
    }

    @Override protected void onDraw(Canvas canvas) {
        int w = Native.frameWidth(), h = Native.frameHeight();
        if (w > 0 && h > 0) {
            if (bmp == null || bmp.getWidth() != w || bmp.getHeight() != h)
                bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            if (Native.copyFrame(bmp)) {
                /* One scale for every frame width (the centre 256 never rescales when the width changes at a map); fw is the widest frame the
                 * wide core can send (256 + 2N).  512-wide hi-res frames count as 256.  8:7 pixels unless the mode says otherwise:
                 *   FILL     width = fw exactly, rows cut at the bottom (N=27: 199 of 224 rows, N=46: all of them): no bars, no distortion
                 *   FIT      the whole frame, bars where the aspect differs
                 *   INTEGER  whole-number factors (4 high, 5 wide), centred
                 *   STRETCH  the whole frame over the whole screen (pixels are not 8:7) */
                float W = getWidth(), H = getHeight(), fw = Math.max(256f, Settings.fullW), lw = w == 512 ? 256f : w;
                float pxw, sy, top;
                switch (Settings.fit) {
                    case 0: pxw = Math.min(W / (fw * 8f / 7f), H / REFH) * 8f / 7f; sy = pxw * 7f / 8f; top = (H - h * sy) / 2f; break;
                    case 2: sy = Math.max(1f, (float) Math.floor(H / REFH)); pxw = (float) Math.floor(sy * 8f / 7f + 0.5f);
                            while (pxw * fw > W && sy > 1f) { sy -= 1f; pxw = (float) Math.floor(sy * 8f / 7f + 0.5f); }
                            top = (float) Math.floor((H - h * sy) / 2f); break;
                    case 3: pxw = W / fw; sy = H / h; top = 0f; break;
                    default:
                        if (fw <= 256f) { pxw = Math.min(W / (fw * 8f / 7f), H / REFH) * 8f / 7f; sy = pxw * 7f / 8f; top = (H - h * sy) / 2f; }   /* no extra width: nothing to fill with */
                        else { pxw = W / fw; sy = pxw * 7f / 8f; top = 0f; }
                }
                float cx = W / 2f;
                dst.set(cx - lw / 2f * pxw, top, cx + lw / 2f * pxw, top + h * sy);
                /* Whole screen pixels only: a fractional top/left puts every texel seam (and the first/last row) between two screen rows, which
                 * showed as a faint horizontal line while scrolling and softened the tiles (Thor round 3, items 5 and 6). */
                dst.set(Math.round(dst.left), Math.round(dst.top), Math.round(dst.right), Math.round(dst.bottom));
                /* No filter chosen: from 4x up a texel is wide enough that nearest-neighbour (a row of 5 or 4 px) is crisp and the blended seams of the
                 * sharp-bilinear shader soften about a quarter of the pixels of dithered tiles (log scene, Thor round 3b item 6); the shader only
                 * serves the smaller scales.  A chosen filter is always used. */
                int mode = Settings.shader;
                if (mode == NONE) {
                    boolean crisp = dst.width() / w >= 4f && dst.height() / h >= 4f;
                    if (Settings.fit != 2 && !crisp) mode = SHARP;
                }
                if (mode != NONE && drawShader(canvas, mode, w, h)) { postInvalidateOnAnimation(); return; }
                canvas.drawBitmap(bmp, null, dst, paint);
            }
        }
        postInvalidateOnAnimation();
    }
}
