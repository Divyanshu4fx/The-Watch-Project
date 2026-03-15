package com.divya.watchappnordic.util

import android.app.Activity
import android.content.Context
import android.content.res.ColorStateList
import android.graphics.Color
import android.os.Build
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.EditText
import android.widget.ImageView
import android.widget.ProgressBar
import android.widget.SeekBar
import android.widget.TextView
import androidx.core.view.WindowInsetsControllerCompat
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import com.google.android.material.textfield.TextInputLayout
import com.google.android.material.switchmaterial.SwitchMaterial

object ThemeHelper {
    private const val PREFS_NAME = "ThemePrefs"
    private const val KEY_THEME_COLOR = "theme_color"
    private const val DEFAULT_COLOR = "#41FF00"

    // Tag to prevent tinting certain ImageViews (like app icons)
    const val TAG_KEEP_COLOR = "keep_original_color"

    fun getThemeColor(context: Context): Int {
        return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .getInt(KEY_THEME_COLOR, Color.parseColor(DEFAULT_COLOR))
    }

    fun setThemeColor(context: Context, color: Int) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putInt(KEY_THEME_COLOR, color)
            .apply()
    }

    fun applyThemeToActivity(activity: Activity) {
        val color = getThemeColor(activity)
        applyThemeToWindow(activity.window, color)
        applyTheme(activity.findViewById(android.R.id.content), color)
    }

    fun applyThemeToWindow(window: Window, color: Int) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            window.statusBarColor = Color.BLACK
            window.navigationBarColor = Color.BLACK
        }
        
        val decorView = window.decorView
        val wic = WindowInsetsControllerCompat(window, decorView)
        wic.isAppearanceLightStatusBars = false 
        wic.isAppearanceLightNavigationBars = false
    }

    fun applyTheme(view: View, color: Int) {
        val colorStateList = ColorStateList.valueOf(color)
        val darkColor = darkenColor(color)
        val darkColorStateList = ColorStateList.valueOf(darkColor)

        when (view) {
            is MaterialToolbar -> {
                view.setTitleTextColor(color)
                view.setSubtitleTextColor(darkColor)
                view.setNavigationIconTint(color)
                view.overflowIcon?.setTint(color)
            }
            is MaterialButton -> {
                view.setTextColor(color)
                view.strokeColor = colorStateList
                view.rippleColor = ColorStateList.valueOf(color).withAlpha(30)
                view.iconTint = colorStateList
            }
            is MaterialCardView -> {
                view.strokeColor = darkColor
            }
            is SwitchMaterial -> {
                view.thumbTintList = colorStateList
                view.trackTintList = darkColorStateList
            }
            is SeekBar -> {
                view.thumbTintList = colorStateList
                view.progressTintList = colorStateList
                view.progressBackgroundTintList = darkColorStateList
            }
            is ProgressBar -> {
                view.progressTintList = colorStateList
                view.indeterminateTintList = colorStateList
            }
            is TextInputLayout -> {
                view.setBoxStrokeColorStateList(colorStateList)
                view.hintTextColor = colorStateList
                view.defaultHintTextColor = darkColorStateList
                view.setStartIconTintList(colorStateList)
                view.setEndIconTintList(colorStateList)
            }
            is EditText -> {
                view.setTextColor(color)
                view.setHintTextColor(darkColor)
            }
            is TextView -> {
                view.setTextColor(color)
            }
            is ImageView -> {
                // Only tint if the tag is NOT set (preserve app icons)
                if (view.tag != TAG_KEEP_COLOR) {
                    view.imageTintList = colorStateList
                } else {
                    view.imageTintList = null // Clear any previous tint
                }
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            try {
                view.verticalScrollbarThumbDrawable?.setTint(color)
                view.horizontalScrollbarThumbDrawable?.setTint(color)
            } catch (e: Exception) {}
        }

        if (view is ViewGroup) {
            for (i in 0 until view.childCount) {
                applyTheme(view.getChildAt(i), color)
            }
        }
    }

    // Additional helper functions
    fun getLuminance(color: Int): Double {
        val r = Color.red(color) / 255.0
        val g = Color.green(color) / 255.0
        val b = Color.blue(color) / 255.0
        
        return 0.299 * r + 0.587 * g + 0.114 * b
    }

    fun getContrastColor(backgroundColor: Int): Int {
        return if (getLuminance(backgroundColor) > 0.5) Color.BLACK else Color.WHITE
    }

    private fun darkenColor(color: Int, factor: Float = 0.6f): Int {
        val hsv = FloatArray(3)
        Color.colorToHSV(color, hsv)
        hsv[2] *= factor
        return Color.HSVToColor(hsv)
    }
}
