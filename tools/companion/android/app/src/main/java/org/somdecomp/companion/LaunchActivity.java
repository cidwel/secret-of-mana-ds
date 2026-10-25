package org.somdecomp.companion;

import android.app.Activity;
import android.app.ActivityOptions;
import android.content.Intent;
import android.os.Bundle;
import android.view.Display;

/**
 * Launcher entry (app drawer, ES-DE, monkey).  A launcher on the AYN Thor may start us on the bottom display; the game has to
 * be on the default (top) display, so this invisible activity only forwards to the start menu (StartMenuActivity, which starts MainActivity) with the launch display fixed to
 * the default display and finishes.
 */
public final class LaunchActivity extends Activity {
    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        Intent i = new Intent(this, StartMenuActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (getIntent().getExtras() != null) i.putExtras(getIntent().getExtras());
        ActivityOptions o = ActivityOptions.makeBasic();
        o.setLaunchDisplayId(Display.DEFAULT_DISPLAY);
        startActivity(i, o.toBundle());
        finish();
    }
}
