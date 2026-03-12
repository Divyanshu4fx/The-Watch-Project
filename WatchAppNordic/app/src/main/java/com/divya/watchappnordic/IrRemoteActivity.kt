package com.divya.watchappnordic

import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.google.android.material.button.MaterialButton
import com.google.android.material.textfield.TextInputEditText

class IrRemoteActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_ir_remote)

        val toolbar = findViewById<com.google.android.material.appbar.MaterialToolbar>(R.id.toolbar)
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        // Preset Buttons (Example for LG TV - Protocol 1)
        setupButton(R.id.btnPower, 1, 0x20DF10EF)
        setupButton(R.id.btnMute, 1, 0x20DF906F)
        setupButton(R.id.btnVolUp, 1, 0x20DF40BF)
        setupButton(R.id.btnVolDown, 1, 0x20DFC03F)
        setupButton(R.id.btnChUp, 1, 0x20DF00FF)
        setupButton(R.id.btnChDown, 1, 0x20DF807F)
        setupButton(R.id.btnSource, 1, 0x20DFF00F)
        setupButton(R.id.btnMenu, 1, 0x20DFC23D)

        val etCustomHex = findViewById<TextInputEditText>(R.id.etCustomHex)
        val btnSendCustom = findViewById<MaterialButton>(R.id.btnSendCustom)
        val btnDone = findViewById<MaterialButton>(R.id.btnDone)

        btnSendCustom.setOnClickListener {
            val hexString = etCustomHex.text.toString().removePrefix("0x").removePrefix("0X")
            try {
                val hexCode = hexString.toLong(16)
                sendIr(1, hexCode) // Default to protocol 1
            } catch (e: Exception) {
                Toast.makeText(this, "INVALID_HEX_FORMAT", Toast.LENGTH_SHORT).show()
            }
        }

        btnDone.setOnClickListener {
            finish()
        }
    }

    private fun setupButton(id: Int, protocol: Int, hexCode: Long) {
        findViewById<MaterialButton>(id).setOnClickListener {
            sendIr(protocol, hexCode)
        }
    }

    private fun sendIr(protocol: Int, hexCode: Long) {
        MainActivity.peripheral?.let { manager ->
            if (manager.isConnected) {
                manager.sendIrCommand(protocol, hexCode)
                Toast.makeText(this, "IR_SENT: 0x${hexCode.toString(16).uppercase()}", Toast.LENGTH_SHORT).迅show()
            } else {
                Toast.makeText(this, "UNIT_DISCONNECTED", Toast.LENGTH_SHORT).show()
            }
        }
    }

    override fun onSupportNavigateUp(): Boolean {
        finish()
        return true
    }
}
