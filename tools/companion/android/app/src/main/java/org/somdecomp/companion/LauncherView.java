package org.somdecomp.companion;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

/** Start menu surface: the shared C renderer draws into a bitmap of the view's size; touches become taps or list scrolls. */
final class LauncherView extends View {
    private Bitmap bmp;
    private final Paint paint = new Paint();
    private int shownRev = -1;
    private float downX, downY, lastY;
    private static long smallest = Long.MAX_VALUE;   /* the model's row count follows the smallest screen showing the menu */
    private boolean dragging;
    private float dragAcc;
    private final boolean top;   /* the top screen: shows the wallpaper / the summary, takes no touches */

    LauncherView(Context c) { this(c, false); }

    LauncherView(Context c, boolean top) {
        super(c);
        this.top = top;
        setBackgroundColor(0xFF000000);
        paint.setFilterBitmap(false);
        setFocusable(true);
    }

    @Override protected void onSizeChanged(int w, int h, int ow, int oh) {
        bmp = w > 0 && h > 0 ? Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888) : null;
        shownRev = -1;
        long area = (long) w * h;
        if (!top && area > 0 && area <= smallest) { smallest = area; LauncherNative.setView(w, h); }
    }

    @Override protected void onDetachedFromWindow() { if (!top) smallest = Long.MAX_VALUE; super.onDetachedFromWindow(); }

    private static Bitmap wall;
    private static boolean wallTried;

    /** The original Secret of Mana key art (assets/som_bg.jpg, staged by gradle from the user's own file, never committed), centre-cropped to the view. */
    private boolean drawWallpaper() {
        if (!wallTried) {
            wallTried = true;
            try (java.io.InputStream in = getContext().getAssets().open("som_bg.jpg")) { wall = android.graphics.BitmapFactory.decodeStream(in); }
            catch (Exception ignored) { wall = null; }
        }
        if (wall == null || bmp == null) return false;
        int w = bmp.getWidth(), h = bmp.getHeight();
        float k = Math.max((float) w / wall.getWidth(), (float) h / wall.getHeight());
        int sw = Math.min(wall.getWidth(), Math.round(w / k)), sh = Math.min(wall.getHeight(), Math.round(h / k));
        android.graphics.Rect src = new android.graphics.Rect((wall.getWidth() - sw) / 2, (wall.getHeight() - sh) / 2, (wall.getWidth() + sw) / 2, (wall.getHeight() + sh) / 2);
        Paint p = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
        new Canvas(bmp).drawBitmap(wall, src, new android.graphics.Rect(0, 0, w, h), p);
        return true;
    }

    /** Called about 30 times a second: redraws only when the model changed. */
    void refresh() {
        int r = top ? LauncherNative.topRevision() : LauncherNative.revision();
        if (r != shownRev && bmp != null) {
            shownRev = r;
            if (top && r >= 1000000 && drawWallpaper()) { invalidate(); return; }   /* main menu and "starting the game": the key art, no procedural wallpaper */
            if (top ? !LauncherNative.renderTop(bmp) : !LauncherNative.render(bmp)) android.util.Log.w("somcompanion", "menu render failed " + bmp.getWidth() + "x" + bmp.getHeight());
            invalidate();
        }
    }

    @Override protected void onDraw(Canvas canvas) {
        if (bmp != null) canvas.drawBitmap(bmp, 0, 0, paint);
    }

    @Override public boolean onTouchEvent(MotionEvent e) {
        if (top) return true;
        int w = getWidth(), h = getHeight();
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downX = e.getX(); downY = lastY = e.getY(); dragging = false; dragAcc = 0;
                return true;
            case MotionEvent.ACTION_MOVE: {
                float dy = e.getY() - lastY;
                if (!dragging && Math.abs(e.getY() - downY) > 24) dragging = true;
                if (dragging) {
                    dragAcc -= dy;
                    int rh = Math.max(1, LauncherNative.rowHeightPx(w, h));
                    int rows = (int) (dragAcc / rh);
                    if (rows != 0) { LauncherNative.scroll(rows); dragAcc -= rows * rh; }
                }
                lastY = e.getY();
                return true;
            }
            case MotionEvent.ACTION_UP:
                if (!dragging) {
                    final int tx = (int) downX, ty = (int) downY;
                    if (StartMenuActivity.current != null) StartMenuActivity.current.input(false, () -> LauncherNative.tap(tx, ty, w, h));
                    else LauncherNative.tap(tx, ty, w, h);
                }
                refresh();
                return true;
            default:
                return true;
        }
    }
}
