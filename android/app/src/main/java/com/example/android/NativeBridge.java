package com.example.android;

import android.content.res.AssetManager;

public final class NativeBridge {
    static {
        System.loadLibrary("LoppeBrowser");
    }

    private NativeBridge() {
    }

    public static native void configurePlatform(AssetManager assetManager, String filesDir);
    public static native void loadUi(String assetPath);
    public static native void init();
    public static native void onSurfaceCreated();
    public static native void onPause();
    public static native void onResume();
    public static native void resize(int width, int height, float scaleX, float scaleY);
    public static native void step();
    public static native void clearPointerState();
    public static native void onTouch(int pointerId, int action, float x, float y);
    public static native void onScroll(float dx, float dy);
    public static native void onKey(int keyCode, int action, int metaState, int unicodeCodePoint);
    public static native boolean isTextInputActive();
    public static native boolean isTextInputMultiline();
    public static native boolean handleBackPressed();
}