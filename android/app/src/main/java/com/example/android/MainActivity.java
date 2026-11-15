package com.example.android;

import android.app.Activity;
import android.os.Bundle;

public class MainActivity extends Activity {
    private LoppeSurfaceView view;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(Bundle.EMPTY);

        LoppePlatform.initialize(this);

        NativeBridge.configurePlatform(getAssets(), getFilesDir().getAbsolutePath());
        NativeBridge.loadUi("ui/main.lui");

        view = new LoppeSurfaceView(this);
        setContentView(view);
    }

    @Override
    protected void onPause() {
        super.onPause();
        NativeBridge.onPause();
        if (view != null) {
            view.onPause();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        NativeBridge.onResume();
        if (view != null) {
            view.onResume();
        }
    }
}