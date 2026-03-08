package com.divya.watchappnordic

import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity

class AlarmActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_alarm)

        setupAlarmItem(R.id.alarm1, 0, "Alarm 1")
        setupAlarmItem(R.id.alarm2, 1, "Alarm 2")
        setupAlarmItem(R.id.alarm3, 2, "Alarm 3")
        setupAlarmItem(R.id.alarm4, 3, "Alarm 4")
        setupAlarmItem(R.id.alarm5, 4, "Alarm 5")
    }

    private fun setupAlarmItem(layoutId: Int, index: Int, label: String) {
        val layout = findViewById<View>(layoutId)
        val tvLabel = layout.findViewById<TextView>(R.id.tvAlarmLabel)
        val etHour = layout.findViewById<EditText>(R.id.etHour)
        val etMinute = layout.findViewById<EditText>(R.id.etMinute)
        val etMessage = layout.findViewById<EditText>(R.id.etMessage)
        val btnSet = layout.findViewById<Button>(R.id.btnSet)

        tvLabel.text = label

        btnSet.setOnClickListener {
            val hourStr = etHour.text.toString()
            val minuteStr = etMinute.text.toString()
            val message = etMessage.text.toString()

            if (hourStr.isEmpty() || minuteStr.isEmpty()) {
                Toast.makeText(this, "Please enter hour and minute", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            val hour = hourStr.toInt()
            val minute = minuteStr.toInt()

            if (hour !in 0..23 || minute !in 0..59) {
                Toast.makeText(this, "Invalid time", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            MainActivity.peripheral?.let { manager ->
                manager.setAlarm(index, hour, minute, message)
                Toast.makeText(this, "Alarm $index set for $hour:$minute", Toast.LENGTH_SHORT).show()
            } ?: run {
                Toast.makeText(this, "Watch not connected", Toast.LENGTH_SHORT).show()
            }
        }
    }
}
