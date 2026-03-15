package com.divya.watchappnordic

import android.graphics.Color
import android.os.Bundle
import android.view.View
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.divya.watchappnordic.util.ThemeHelper
import com.google.android.material.button.MaterialButton

class ThemeConfigActivity : AppCompatActivity() {

    private lateinit var viewColorPreview: View
    private lateinit var tvHexValue: TextView
    private lateinit var sbRed: SeekBar
    private lateinit var sbGreen: SeekBar
    private lateinit var sbBlue: SeekBar
    
    private var currentColor: Int = Color.parseColor("#41FF00")

    override fun onCreate(savedInstanceState: Bundle?) {
        currentColor = ThemeHelper.getThemeColor(this)
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

        sbRed.progress = Color.red(currentColor)
        sbGreen.progress = Color.green(currentColor)
        sbBlue.progress = Color.blue(currentColor)
        
        // Apply theme immediately after loading views
        ThemeHelper.applyThemeToActivity(this)
        
        // Initial theme application (current saved theme)
        updatePreviewOnly(currentColor)

        val listener = object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                val r = sbRed.progress
                val g = sbGreen.progress
                val b = sbBlue.progress
                currentColor = Color.rgb(r, g, b)
                updatePreviewOnly(currentColor)
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        }

        sbRed.setOnSeekBarChangeListener(listener)
        sbGreen.setOnSeekBarChangeListener(listener)
        sbBlue.setOnSeekBarChangeListener(listener)

        findViewById<MaterialButton>(R.id.btnApplyTheme).setOnClickListener {
            // 1. Save locally
            ThemeHelper.setThemeColor(this, currentColor)
            
            // 2. Apply to current UI immediately
            ThemeHelper.applyThemeToActivity(this)
            
            // 3. Send to Watch
            MainActivity.peripheral?.let { manager ->
                if (manager.isConnected) {
                    manager.sendColor(currentColor)
                    Toast.makeText(this, "FACE_SYNC_SUCCESS", Toast.LENGTH_SHORT).show()
                } else {
                    Toast.makeText(this, "UNIT_OFFLINE_WATCH_SYNC_FAILED", Toast.LENGTH_SHORT).show()
                }
            }
            Toast.makeText(this, "SYSTEM_THEME_UPDATED", Toast.LENGTH_SHORT).show()
        }

        findViewById<MaterialButton>(R.id.btnDone).setOnClickListener {
            finish()
        }
    }

    private fun updatePreviewOnly(color: Int) {
        viewColorPreview.setBackgroundColor(color)
        tvHexValue.text = String.format("HEX: #%02X%02X%02X", Color.red(color), Color.green(color), Color.blue(color))
        tvHexValue.setTextColor(color)
    }

    override fun onSupportNavigateUp(): Boolean {
        finish()
        return true
    }
}
