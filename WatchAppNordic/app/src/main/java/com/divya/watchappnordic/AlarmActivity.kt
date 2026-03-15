package com.divya.watchappnordic

import android.os.Bundle
import android.view.LayoutInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.divya.watchappnordic.util.ThemeHelper
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.floatingactionbutton.FloatingActionButton
import com.google.android.material.switchmaterial.SwitchMaterial
import com.google.android.material.textfield.TextInputEditText

data class Alarm(
    val id: Int,
    var hour: Int,
    var minute: Int,
    var message: String,
    var isEnabled: Boolean = true
) {
    val timeString: String
        get() = String.format("%02d:%02d", hour, minute)
}

class AlarmActivity : AppCompatActivity() {

    private val alarms = mutableListOf<Alarm>()
    private lateinit var adapter: AlarmAdapter
    private val MAX_ALARMS = 5
    private var activeThemeColor: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        activeThemeColor = ThemeHelper.getThemeColor(this)
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_alarm)

        val toolbar = findViewById<MaterialToolbar>(R.id.toolbar)
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        supportActionBar?.setDisplayShowHomeEnabled(true)

        // Apply theme immediately
        ThemeHelper.applyThemeToActivity(this)

        val rvAlarms = findViewById<RecyclerView>(R.id.rvAlarms)
        val fabAddAlarm = findViewById<FloatingActionButton>(R.id.fabAddAlarm)
        val btnDone = findViewById<Button>(R.id.btnDone)

        adapter = AlarmAdapter(alarms, 
            onToggle = { alarm -> sendAlarmToWatch(alarm) },
            onClick = { alarm -> showEditDialog(alarm) }
        )
        
        rvAlarms.layoutManager = LinearLayoutManager(this)
        rvAlarms.adapter = adapter

        fabAddAlarm.setOnClickListener {
            if (alarms.size < MAX_ALARMS) {
                showAddDialog()
            } else {
                Toast.makeText(this, "Maximum 5 alarms reached", Toast.LENGTH_SHORT).show()
            }
        }

        btnDone.setOnClickListener {
            finish()
        }

        setupBleObserver()
        fetchAlarmsFromWatch()
    }

    private fun setupBleObserver() {
        MainActivity.peripheral?.onAlarmReceived = { index, hour, minute, message, enabled ->
            runOnUiThread {
                val existingAlarm = alarms.find { it.id == index }
                if (existingAlarm != null) {
                    existingAlarm.hour = hour
                    existingAlarm.minute = minute
                    existingAlarm.message = message
                    existingAlarm.isEnabled = enabled
                } else {
                    alarms.add(Alarm(index, hour, minute, message, enabled))
                    alarms.sortBy { it.id }
                }
                adapter.notifyDataSetChanged()
            }
        }
    }

    private fun fetchAlarmsFromWatch() {
        MainActivity.peripheral?.let { manager ->
            manager.requestAlarms()
            Toast.makeText(this, "Fetching alarms...", Toast.LENGTH_SHORT).show()
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == android.R.id.home) {
            onBackPressedDispatcher.onBackPressed()
            return true
        }
        return super.onOptionsItemSelected(item)
    }

    private fun showAddDialog() {
        val newAlarm = Alarm(alarms.size, 8, 0, "New Alarm")
        showEditDialog(newAlarm, isNew = true)
    }

    private fun showEditDialog(alarm: Alarm, isNew: Boolean = false) {
        val dialogView = LayoutInflater.from(this).inflate(R.layout.dialog_edit_alarm, null)
        val timePicker = dialogView.findViewById<android.widget.TimePicker>(R.id.timePicker)
        val etMessage = dialogView.findViewById<TextInputEditText>(R.id.etMessage)

        timePicker.setIs24HourView(true)
        timePicker.hour = alarm.hour
        timePicker.minute = alarm.minute
        etMessage.setText(alarm.message)

        val dialog = AlertDialog.Builder(this, R.style.TerminalDialogTheme)
            .setTitle(if (isNew) "Add Alarm" else "Edit Alarm")
            .setView(dialogView)
            .setPositiveButton("Save") { _, _ ->
                alarm.hour = timePicker.hour
                alarm.minute = timePicker.minute
                alarm.message = etMessage.text.toString()
                alarm.isEnabled = true 

                if (isNew) {
                    alarms.add(alarm)
                    alarms.sortBy { it.id }
                }
                
                adapter.notifyDataSetChanged()
                sendAlarmToWatch(alarm)
            }
            .setNegativeButton("Cancel", null)
            .create()
            
        dialog.show()
        ThemeHelper.applyTheme(dialogView, activeThemeColor)
        // Also apply theme to dialog window views
        dialog.window?.decorView?.let { ThemeHelper.applyTheme(it, activeThemeColor) }
    }

    private fun sendAlarmToWatch(alarm: Alarm) {
        MainActivity.peripheral?.let { manager ->
            manager.setAlarm(alarm.id, alarm.hour, alarm.minute, alarm.message, alarm.isEnabled)
        } ?: run {
            Toast.makeText(this, "Watch not connected", Toast.LENGTH_SHORT).show()
        }
    }

    inner class AlarmAdapter(
        private val list: List<Alarm>,
        private val onToggle: (Alarm) -> Unit,
        private val onClick: (Alarm) -> Unit
    ) : RecyclerView.Adapter<AlarmAdapter.ViewHolder>() {

        inner class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
            val tvTime: TextView = view.findViewById(R.id.tvTime)
            val tvMessage: TextView = view.findViewById(R.id.tvMessage)
            val switchAlarm: SwitchMaterial = view.findViewById(R.id.switchAlarm)

            init {
                view.setOnClickListener { 
                    val pos = adapterPosition
                    if (pos != RecyclerView.NO_POSITION) {
                        onClick(list[pos])
                    }
                }
            }
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val view = LayoutInflater.from(parent.context).inflate(R.layout.item_alarm, parent, false)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val alarm = list[position]
            holder.tvTime.text = alarm.timeString
            holder.tvMessage.text = alarm.message
            
            ThemeHelper.applyTheme(holder.itemView, activeThemeColor)

            holder.switchAlarm.setOnCheckedChangeListener(null)
            holder.switchAlarm.isChecked = alarm.isEnabled
            holder.switchAlarm.setOnCheckedChangeListener { _, isChecked ->
                alarm.isEnabled = isChecked
                onToggle(alarm)
            }
        }

        override fun getItemCount() = list.size
    }
}
