package org.somdecomp.companion;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

/** Bottom screen: the context-aware SoM-style UX rendered by the shared C layer at the view's own pixel size. */
final class StatusView extends View {
    /* The picture is rendered on a worker thread into one of two bitmaps and handed to the UI thread only when it differs
     * from the previous one, so the UI thread (shared with the game view) never pays for the layout or the 5 MB upload
     * of an unchanged screen. */
    private final Bitmap[] bmps = new Bitmap[2];
    private Bitmap bmp;                 /* the one on show */
    private final Paint paint = new Paint();
    private android.os.HandlerThread worker;
    private android.os.Handler wh;
    private int back;
    private final Object lock = new Object();

    StatusView(Context c) {
        super(c);
        setBackgroundColor(0xFF000000);
        paint.setFilterBitmap(false);
    }

    private final Runnable render = new Runnable() {
        @Override public void run() {
            int w = getWidth(), h = getHeight();
            if (w > 0 && h > 0) {
                if (bmps[back] == null || bmps[back].getWidth() != w || bmps[back].getHeight() != h) bmps[back] = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
                final Bitmap b = bmps[back];
                if (Native.renderUx(b)) {
                    synchronized (lock) { bmp = b; back ^= 1; }
                    postInvalidateOnAnimation();
                }
            }
            if (wh != null) wh.postDelayed(this, 33);   /* up to 30 checks a second; nothing is drawn unless the picture changed */
        }
    };

    @Override protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        worker = new android.os.HandlerThread("somux");
        worker.start();
        wh = new android.os.Handler(worker.getLooper());
        wh.post(render);
    }

    @Override protected void onDetachedFromWindow() {
        if (wh != null) wh.removeCallbacksAndMessages(null);
        if (worker != null) worker.quitSafely();
        wh = null; worker = null;
        super.onDetachedFromWindow();
    }

    @Override protected void onDraw(Canvas canvas) {
        synchronized (lock) { if (bmp != null) canvas.drawBitmap(bmp, 0, 0, paint); }
    }

    private float downX, downY;
    private boolean fired;
    private final Runnable lp = new Runnable() {
        @Override public void run() {
            
            fired = true;
            Native.longPressUx((int) downX, (int) downY, getWidth(), getHeight());
        }
    };

    /** A tap runs on release; holding an entry for 0.5 s opens the bind screen ( see the UX layer) instead. */
    @Override public boolean onTouchEvent(MotionEvent e) {
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downX = e.getX(); downY = e.getY(); fired = false; postDelayed(lp, 500);
                if (Native.pressUx((int) downX, (int) downY, getWidth(), getHeight(), true) && Settings.current != null) Settings.current.press();
                break;
            case MotionEvent.ACTION_MOVE:
                Native.moveUx((int) e.getX(), (int) e.getY(), getWidth(), getHeight());
                if (Math.abs(e.getX() - downX) > 30 || Math.abs(e.getY() - downY) > 30) removeCallbacks(lp);
                break;
            case MotionEvent.ACTION_UP:
                removeCallbacks(lp);
                Native.pressUx((int) downX, (int) downY, getWidth(), getHeight(), false);
                if (!fired && Math.abs(e.getX() - downX) <= 30 && Math.abs(e.getY() - downY) <= 30)
                    Native.touchUx((int) downX, (int) downY, getWidth(), getHeight());
                break;
            case MotionEvent.ACTION_CANCEL: removeCallbacks(lp); Native.pressUx(0, 0, getWidth(), getHeight(), false); break;
            default: break;
        }
        return true;
    }
}
