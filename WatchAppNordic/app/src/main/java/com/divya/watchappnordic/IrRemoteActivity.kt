package com.divya.watchappnordic

import android.content.Context
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.divya.watchappnordic.util.ThemeHelper
import com.google.android.material.button.MaterialButton
import com.google.android.material.textfield.TextInputEditText
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import java.util.Collections

data class CustomIrButton(val label: String, val hexCode: Long)

class IrRemoteActivity : AppCompatActivity() {

    private lateinit var tvLastCommand: TextView
    private lateinit var rvCustomButtons: RecyclerView
    private lateinit var adapter: CustomButtonAdapter
    private val customButtons = mutableListOf<CustomIrButton>()
    private val gson = Gson()
    private val PREFS_NAME = "IrPrefs"
    private val KEY_CUSTOM_BUTTONS = "custom_buttons"
    private var activeThemeColor: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        activeThemeColor = ThemeHelper.getThemeColor(this)
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_ir_remote)

        val toolbar = findViewById<com.google.android.material.appbar.MaterialToolbar>(R.id.toolbar)
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        // Apply theme immediately after loading views
        ThemeHelper.applyThemeToActivity(this)

        tvLastCommand = findViewById(R.id.tvLastCommand)
        rvCustomButtons = findViewById(R.id.rvCustomButtons)

        // Preset Buttons
        setupButton(R.id.btnPower, 1, 0x20DF10EF)
        setupButton(R.id.btnMute, 1, 0x20DF906F)
        setupButton(R.id.btnVolUp, 1, 0x20DF40BF)
        setupButton(R.id.btnVolDown, 1, 0x20DFC03F)
        setupButton(R.id.btnChUp, 1, 0x20DF00FF)
        setupButton(R.id.btnChDown, 1, 0x20DF807F)
        setupButton(R.id.btnSource, 1, 0x20DFF00F)
        setupButton(R.id.btnMenu, 1, 0x20DFC23D)

        loadCustomButtons()
        adapter = CustomButtonAdapter(customButtons)
        rvCustomButtons.layoutManager = LinearLayoutManager(this)
        rvCustomButtons.adapter = adapter

        // Drag and Drop implementation
        val itemTouchHelper = ItemTouchHelper(object : ItemTouchHelper.SimpleCallback(ItemTouchHelper.UP or ItemTouchHelper.DOWN, ItemTouchHelper.LEFT or ItemTouchHelper.RIGHT) {
            override fun onMove(recyclerView: RecyclerView, viewHolder: RecyclerView.ViewHolder, target: RecyclerView.ViewHolder): Boolean {
                val fromPos = viewHolder.adapterPosition
                val toPos = target.adapterPosition
                Collections.swap(customButtons, fromPos, toPos)
                adapter.notifyItemMoved(fromPos, toPos)
                saveCustomButtons()
                return true
            }

            override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) {
                val position = viewHolder.adapterPosition
                customButtons.removeAt(position)
                adapter.notifyItemRemoved(position)
                saveCustomButtons()
                tvLastCommand.text = "[OP] COMMAND_DELETED"
            }
        })
        itemTouchHelper.attachToRecyclerView(rvCustomButtons)

        findViewById<MaterialButton>(R.id.btnAddCustomButton).setOnClickListener {
            showAddButtonDialog()
        }

        val etCustomHex = findViewById<TextInputEditText>(R.id.etCustomHex)
        val btnSendCustom = findViewById<MaterialButton>(R.id.btnSendCustom)
        val btnDone = findViewById<MaterialButton>(R.id.btnDone)

        etCustomHex.setText("0x")
        etCustomHex.setSelection(2)
        etCustomHex.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: Editable?) {
                if (s != null && !s.startsWith("0x")) {
                    etCustomHex.setText("0x")
                    etCustomHex.setSelection(etCustomHex.text?.length ?: 2)
                }
            }
        })

        btnSendCustom.setOnClickListener {
            val hexString = etCustomHex.text.toString().removePrefix("0x").removePrefix("0X").trim()
            try {
                if (hexString.isNotEmpty()) {
                    val hexCode = hexString.toLong(16)
                    sendIr(1, hexCode)
                } else {
                    tvLastCommand.text = "[ERR] ENTER_HEX_VALUE"
                }
            } catch (e: Exception) {
                tvLastCommand.text = "[ERR] INVALID_HEX_FORMAT"
            }
        }

        btnDone.setOnClickListener {
            finish()
        }
    }

    private fun showAddButtonDialog() {
        val view = LayoutInflater.from(this).inflate(R.layout.dialog_add_ir_button, null)
        val etLabel = view.findViewById<TextInputEditText>(R.id.etLabel)
        val etHex = view.findViewById<TextInputEditText>(R.id.etHex)
        etHex.setText("0x")

        val dialog = AlertDialog.Builder(this, R.style.TerminalDialogTheme)
            .setTitle("[ADD_REMOTE_MODULE]")
            .setView(view)
            .setPositiveButton("SAVE") { _, _ ->
                val label = etLabel.text.toString().trim()
                val hexStr = etHex.text.toString().removePrefix("0x").removePrefix("0X").trim()
                try {
                    if (label.isNotEmpty() && hexStr.isNotEmpty()) {
                        val hexCode = hexStr.toLong(16)
                        customButtons.add(CustomIrButton(label, hexCode))
                        adapter.notifyItemInserted(customButtons.size - 1)
                        saveCustomButtons()
                    }
                } catch (e: Exception) {
                    tvLastCommand.text = "[ERR] INVALID_HEX_DATA"
                }
            }
            .setNegativeButton("CANCEL", null)
            .create()
            
        dialog.show()
        ThemeHelper.applyTheme(view, activeThemeColor)
        // Also apply theme to dialog window views
        dialog.window?.decorView?.let { ThemeHelper.applyTheme(it, activeThemeColor) }
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
                tvLastCommand.text = "[LAST_OP] SENT: 0x${hexCode.toString(16).uppercase()}"
            } else {
                tvLastCommand.text = "[ERR] UNIT_DISCONNECTED"
            }
        } ?: run {
            tvLastCommand.text = "[ERR] NO_CONNECTION"
        }
    }

    private fun loadCustomButtons() {
        val prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val json = prefs.getString(KEY_CUSTOM_BUTTONS, null)
        if (json != null) {
            val type = object : TypeToken<List<CustomIrButton>>() {}.type
            val list: List<CustomIrButton> = gson.fromJson(json, type)
            customButtons.clear()
            customButtons.addAll(list)
        }
    }

    private fun saveCustomButtons() {
        val prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val json = gson.toJson(customButtons)
        prefs.edit().putString(KEY_CUSTOM_BUTTONS, json).apply()
    }

    override fun onSupportNavigateUp(): Boolean {
        finish()
        return true
    }

    inner class CustomButtonAdapter(private val items: List<CustomIrButton>) : RecyclerView.Adapter<CustomButtonAdapter.ViewHolder>() {
        inner class ViewHolder(v: View) : RecyclerView.ViewHolder(v) {
            val btn: MaterialButton = v.findViewById(R.id.btnCustomRemote)
        }
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val v = LayoutInflater.from(parent.context).inflate(R.layout.item_custom_ir_button, parent, false)
            return ViewHolder(v)
        }
        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val item = items[position]
            holder.btn.text = item.label
            ThemeHelper.applyTheme(holder.itemView, activeThemeColor)
            holder.btn.setOnClickListener { sendIr(1, item.hexCode) }
        }
        override fun getItemCount() = items.size
    }
}
