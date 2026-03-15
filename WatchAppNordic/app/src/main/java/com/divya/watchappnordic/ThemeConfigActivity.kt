package com.divya.watchappnordic

import android.content.Context
import android.graphics.Color
import android.os.Bundle
import android.view.View
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.button.MaterialButton

class ThemeConfigActivity : AppCompatActivity() {

    private lateinit var viewColorPreview: View
    private lateinit var tvHexValue: TextView
    private lateinit var sbRed: SeekBar
    private lateinit var sbGreen: SeekBar
    private lateinit var sbBlue: SeekBar
    
    private val PREFS_NAME = "ThemePrefs"
    private val KEY_THEME_COLOR = "theme_color"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_theme_config)

        val toolbar = findViewById<com.google.android.material.appbar.MaterialToolbar>(R.id.toolbar)
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        viewColorPreview = findViewById(R.id.viewColorPreview)
        tvHexValue = findViewById(R.id.tvHexValue)
        sbRed = findViewById(R.id.sbRed)
        sbGreen = findViewById(R.id.sbGreen)
        sbBlue = findViewById(R.id.sbBlue)

        val savedColor = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .getInt(KEY_THEME_COLOR, Color.parseColor("#41FF00"))

        sbRed.progress = Color.red(savedColor)
        sbGreen.progress = Color.green(savedColor)
        sbBlue.progress = Color.blue(savedColor)
        updateColorPreview()

        val listener = object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                updateColorPreview()
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        }

        sbRed.setOnSeekBarChangeListener(listener)
        sbGreen.setOnSeekBarChangeListener(listener)
        sbBlue.setOnSeekBarChangeListener(listener)

        findViewById<MaterialButton>(R.id.btnApplyTheme).setOnClickListener {
            val color = Color.rgb(sbRed.progress, sbGreen.progress, sbBlue.progress)
            applyTheme(color)
        }

        findViewById<MaterialButton>(R.id.btnDone).setOnClickListener {
            finish()
        }
    }

    private fun updateColorPreview() {
        val r = sbRed.progress
        val g = sbGreen.progress
        val b = sbBlue.progress
        val color = Color.rgb(r, g, b)
        viewColorPreview.setBackgroundColor(color)
        tvHexValue.text = String.format("HEX: #%02X%02X%02X", r, g, b)
    }

    private fun applyTheme(color: Int) {
        // 1. Send to Watch
        MainActivity.peripheral?.let { manager ->
            if (manager.isConnected) {
                manager.sendColor(color)
                Toast.makeText(this, "FACE_SYNC_INITIATED", Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(this, "UNIT_OFFLINE", Toast.LENGTH_SHORT).show()
            }
        }

        // 2. Save locally for App Theme
        getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putInt(KEY_THEME_COLOR, color)
            .apply()
            
        Toast.makeText(this, "RESTART_REQUIRED_FOR_APP_THEME", Toast.LENGTH_LONG).show()
    }

    override fun onSupportNavigateUp(): Boolean {
        finish()
        return true
    }
}
