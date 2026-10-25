package org.somdecomp.companion;

import android.app.Presentation;
import android.content.Context;
import android.os.Bundle;
import android.view.Display;

/** Window on the second physical screen (AYN Thor: "Screen-2", FLAG_PRESENTATION). */
final class StatusPresentation extends Presentation {
    StatusPresentation(Context outer, Display display) { super(outer, display); }

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        android.widget.LinearLayout l = new android.widget.LinearLayout(getContext());
        l.setOrientation(android.widget.LinearLayout.VERTICAL);
        l.setBackgroundColor(0xFF000000);
        l.addView(new StatusView(getContext()), new android.widget.LinearLayout.LayoutParams(-1, 0, 1f));
        setContentView(l);
    }
}
