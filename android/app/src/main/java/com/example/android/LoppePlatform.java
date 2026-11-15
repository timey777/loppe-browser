package com.example.android;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import java.lang.ref.WeakReference;

/**
 * 平台侧的小事：剪贴板 + 软键盘（IME）。窗口是全屏无标题（见 AndroidManifest 的 Theme），
 * 需要别的原生能力就在这里加静态方法，再从 C++（platform/android_platform.cpp）用 JNI 调。
 */
public final class LoppePlatform {
    private static Context appContext;
    private static WeakReference<LoppeSurfaceView> inputViewRef = new WeakReference<>(null);

    private LoppePlatform() {
    }

    public static void initialize(Context context) {
        appContext = context.getApplicationContext();
    }

    public static void registerInputView(LoppeSurfaceView view) {
        inputViewRef = new WeakReference<>(view);
    }

    public static void setClipboardText(String text) {
        if (appContext == null) {
            return;
        }
        ClipboardManager clipboardManager =
                (ClipboardManager) appContext.getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboardManager == null) {
            return;
        }
        clipboardManager.setPrimaryClip(ClipData.newPlainText("loppe", text));
    }

    public static String getClipboardText() {
        if (appContext == null) {
            return "";
        }
        ClipboardManager clipboardManager =
                (ClipboardManager) appContext.getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboardManager == null || !clipboardManager.hasPrimaryClip()) {
            return "";
        }
        ClipData clipData = clipboardManager.getPrimaryClip();
        if (clipData == null || clipData.getItemCount() == 0) {
            return "";
        }
        CharSequence text = clipData.getItemAt(0).coerceToText(appContext);
        return text != null ? text.toString() : "";
    }

    public static void hideSoftKeyboard(View view) {
        if (appContext == null || view == null) {
            return;
        }
        InputMethodManager imm =
                (InputMethodManager) appContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) {
            imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
        }
    }

    // 引擎侧焦点落到输入控件（input/textarea）时由 C++ 调：按需弹/收软键盘。
    public static void syncSoftInputState(boolean active, boolean multiline) {
        final LoppeSurfaceView view = inputViewRef.get();
        if (view == null) {
            return;
        }

        view.post(() -> {
            view.setNativeTextInputState(active, multiline);

            InputMethodManager imm =
                    (InputMethodManager) view.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm == null) {
                return;
            }

            imm.restartInput(view);
            if (active) {
                if (!view.hasFocus()) {
                    view.requestFocus();
                }
                imm.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT);
            } else {
                imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
        });
    }
}
