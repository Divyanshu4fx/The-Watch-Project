package com.divya.watchappnordic.util

import android.content.Context
import android.content.res.ColorStateList
import android.graphics.Color
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.ImageView
import android.widget.TextView
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import com.google.android.material.textfield.TextInputLayout
import com.google.android.material.switchmaterial.SwitchMaterial

object ThemeHelper {
    private const val PREFS_NAME = "ThemePrefs"
    private const val KEY_THEME_COLOR = "theme_color"
    private const val DEFAULT_COLOR = "#41FF00"

    fun getThemeColor(context: Context): Int {
        return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .getInt(KEY_THEME_COLOR, Color.parseColor(DEFAULT_COLOR))
    }

    fun applyTheme(view: View, color: Int) {
        val colorStateList = ColorStateList.valueOf(color)
        val darkColor = darkenColor(color)
        val darkColorStateList = ColorStateList.valueOf(darkColor)

        when (view) {
            is MaterialButton -> {
                view.setTextColor(color)
                if (view.styleAttribute == com.google.android.material.R.attr.materialButtonStyle || 
                    view.styleAttribute == com.google.android.material.R.attr.borderlessButtonStyle) {
                    // Tonal or Text buttons
                } else {
                    // Outlined buttons
                    view.strokeColor = colorStateList
                }
                view.rippleColor = colorStateList.withAlpha(30)
            }
            is MaterialCardView -> {
                view.strokeColor = darkColorStateList
            }
            is SwitchMaterial -> {
                view.thumbTintList = colorStateList
                view.trackTintList = darkColorStateList
            }
            is TextInputLayout -> {
                view.setBoxStrokeColorStateList(colorStateList)
                view.hintTextColor = colorStateList
                view.defaultHintTextColor = darkColorStateList
            }
            is EditText -> {
                view.setTextColor(color)
            }
            is TextView -> {
                // If it's a specific terminal-style status, use full color, else maybe dark
                if (view.alpha == 1.0f) {
                    view.setTextColor(color)
                } else {
                    view.setTextColor(darkColor)
                }
            }
            is ImageView -> {
                view.imageTintList = colorStateList
            }
        }

        if (view is ViewGroup) {
            for (i in 0 until view.childCount) {
                applyTheme(view.getChildAt(i), color)
            }
        }
    }

    private fun darkenColor(color: Int): Int {
        val hsv = FloatArray(3)
        Color.colorToHSV(color, hsv)
        hsv[2] *= 0.7f // 30% darker
        return Color.HSVToColor(hsv)
    }
}
