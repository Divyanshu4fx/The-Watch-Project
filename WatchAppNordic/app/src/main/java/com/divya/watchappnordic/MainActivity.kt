package com.divya.watchappnordic

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.ColorStateList
import android.graphics.Color
import android.media.AudioAttributes
import android.media.Ringtone
import android.media.RingtoneManager
import android.os.Bundle
import android.os.Build
import android.text.Spannable
import android.text.SpannableString
import android.text.style.ForegroundColorSpan
import android.util.Log
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import androidx.annotation.RequiresApi
import androidx.annotation.RequiresPermission
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.core.widget.NestedScrollView
import com.divya.watchappnordic.service.NotificationCatcherService
import com.divya.watchappnordic.ui.NotificationSettingsActivity
import com.divya.watchappnordic.util.ThemeHelper
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.button.MaterialButton
import no.nordicsemi.android.support.v18.scanner.*
import no.nordicsemi.android.ble.observer.ConnectionObserver
import java.time.Instant

class MainActivity : AppCompatActivity(), ConnectionObserver {

    private lateinit var tvStatus: TextView
    private lateinit var scrollStatus: NestedScrollView
    private lateinit var btnScan: MaterialButton
    private lateinit var btnTimeSync: MaterialButton
    private lateinit var btnSetAlarms: MaterialButton
    private lateinit var btnFindWatch: MaterialButton
    private lateinit var btnIrRemote: MaterialButton
    private lateinit var btnThemeConfig: MaterialButton
    private lateinit var btnNotificationSettings: MaterialButton
    private lateinit var layoutConnectedFeatures: LinearLayout
    private lateinit var toolbar: MaterialToolbar
    
    private val TAG = "BLE_SCANNER"
    private val PREFS_NAME = "WatchPrefs"
    private val KEY_DEVICE_ADDR = "saved_device_addr"
    
    private var ringtone: Ringtone? = null
    private var findPhoneDialog: AlertDialog? = null
    private var activeThemeColor: Int = 0

    companion object {
        var peripheral: MyBleManager? = null
        var isManualLaunch = true // Flag to track if this is the first launch
    }

    @RequiresApi(Build.VERSION_CODES.O)
    override fun onCreate(savedInstanceState: Bundle?) {
        activeThemeColor = ThemeHelper.getThemeColor(this)
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        
        tvStatus = findViewById(R.id.tvStatus)
        scrollStatus = findViewById(R.id.scrollStatus)
        btnScan = findViewById(R.id.btnScan)
        btnTimeSync = findViewById(R.id.btnTimeSync)
        btnSetAlarms = findViewById(R.id.btnSetAlarms)
        btnFindWatch = findViewById(R.id.btnFindWatch)
        btnIrRemote = findViewById(R.id.btnIrRemote)
        btnThemeConfig = findViewById(R.id.btnThemeConfig)
        btnNotificationSettings = findViewById(R.id.btnNotificationSettings)
        layoutConnectedFeatures = findViewById(R.id.layoutConnectedFeatures)
        toolbar = findViewById(R.id.toolbar)

        // Apply theme immediately after loading views
        ThemeHelper.applyThemeToActivity(this)

        btnScan.setOnClickListener {
            Log.d(TAG, "Scan button clicked")
            if (hasPermission()) {
                startBleScan()
            } else {
                requestBlePermission()
            }
        }

        btnTimeSync.setOnClickListener {
            val currentUnixTimeSeconds = Instant.now().epochSecond
            peripheral?.syncTime(currentUnixTimeSeconds)
            updateStatus("[RTC] SYNCED TO $currentUnixTimeSeconds")
        }

        btnSetAlarms.setOnClickListener {
            startActivity(Intent(this, AlarmActivity::class.java))
        }

        btnFindWatch.setOnClickListener {
            showFindWatchDialog()
        }

        btnIrRemote.setOnClickListener {
            startActivity(Intent(this, IrRemoteActivity::class.java))
        }

        btnThemeConfig.setOnClickListener {
            startActivity(Intent(this, ThemeConfigActivity::class.java))
        }

        btnNotificationSettings.setOnClickListener {
            startActivity(Intent(this, NotificationSettingsActivity::class.java))
        }
        
        setupRingtone()
        
        // Auto-connect ONLY if NOT a manual first launch and we have a saved address
        val savedAddr = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).getString(KEY_DEVICE_ADDR, null)
        if (!isManualLaunch && savedAddr != null && (peripheral == null || !peripheral!!.isConnected)) {
            updateStatus("[SYSTEM] RECOVERING_CONNECTION: $savedAddr")
            setButtonDisconnectedState()
            try {
                val device = BluetoothAdapter.getDefaultAdapter().getRemoteDevice(savedAddr)
                connectToDevice(device)
            } catch (e: Exception) {
                updateError("[ERR] RECOVERY_FAILED")
                setButtonInitialState()
            }
        } else if (isManualLaunch) {
            updateStatus("[SYSTEM] COLD_BOOT_COMPLETE")
            updateStatus("[SYSTEM] MANUAL_LINK_REQUIRED")
            isManualLaunch = false // Reset for future background recreations
        }

        // Re-attach listeners if already connected
        peripheral?.let {
            it.setConnectionObserver(this)
            if (it.isConnected) {
                onDeviceConnected(it.bluetoothDevice!!)
            }
        }
    }

    override fun onResume() {
        super.onResume()
        // Always apply the latest theme on resume
        activeThemeColor = ThemeHelper.getThemeColor(this)
        ThemeHelper.applyThemeToActivity(this)
    }

    private fun updateStatus(message: String) {
        runOnUiThread {
            tvStatus.append("\n$message")
            autoScroll()
        }
    }

    private fun updateError(message: String) {
        runOnUiThread {
            val spannable = SpannableString("\n$message")
            spannable.setSpan(
                ForegroundColorSpan(ContextCompat.getColor(this, R.color.terminal_red)),
                0, spannable.length,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE
            )
            tvStatus.append(spannable)
            autoScroll()
        }
    }

    private fun autoScroll() {
        scrollStatus.post {
            scrollStatus.fullScroll(View.FOCUS_DOWN)
        }
    }

    private fun setupRingtone() {
        try {
            val uri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE)
            ringtone = RingtoneManager.getRingtone(applicationContext, uri)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                ringtone?.audioAttributes = AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_ALARM)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to setup ringtone", e)
        }
    }

    private fun showFindPhoneDialog() {
        if (findPhoneDialog?.isShowing == true) return
        
        findPhoneDialog = AlertDialog.Builder(this, R.style.TerminalDialogTheme)
            .setTitle("[PING_RECEIVED]")
            .setMessage("WATCH IS LOOKING FOR THIS UNIT")
            .setPositiveButton("ACKNOWLEDGE") { _, _ ->
                ringtone?.stop()
            }
            .setCancelable(false)
            .create()
        findPhoneDialog?.show()
        // Apply theme to dialog
        findPhoneDialog?.window?.decorView?.let { ThemeHelper.applyTheme(it, activeThemeColor) }
    }

    private fun showFindWatchDialog() {
        peripheral?.findMyWatch(true)
        val dialog = AlertDialog.Builder(this, R.style.TerminalDialogTheme)
            .setTitle("[UNIT_PING_INITIATED]")
            .setMessage("M5_WATCH SHOULD BE VIBRATING")
            .setCancelable(false)
            .setPositiveButton("TERMINATE") { d, _ ->
                peripheral?.findMyWatch(false)
                d.dismiss()
            }
            .create()
        dialog.show()
        // Apply theme to dialog
        dialog.window?.decorView?.let { ThemeHelper.applyTheme(it, activeThemeColor) }
    }

    private fun hasPermission(): Boolean {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        }
        permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        return permissions.all { ActivityCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED }
    }

    private fun requestBlePermission() {
        val permissionsList = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissionsList.add(Manifest.permission.BLUETOOTH_SCAN)
            permissionsList.add(Manifest.permission.BLUETOOTH_CONNECT)
        }
        permissionsList.add(Manifest.permission.ACCESS_FINE_LOCATION)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissionsList.add(Manifest.permission.POST_NOTIFICATIONS)
        }
        ActivityCompat.requestPermissions(this, permissionsList.toTypedArray(), 100)
    }

    private fun startBleScan() {
        val adapter = BluetoothAdapter.getDefaultAdapter()
        if (adapter == null || !adapter.isEnabled) {
            updateError("[ERR] BLUETOOTH_OFF")
            return
        }
        updateStatus("[SCAN] SEARCHING FOR M5_WATCH...")
        val filters = listOf(ScanFilter.Builder().setDeviceName("M5_Watch").build())
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        BluetoothLeScannerCompat.getScanner().startScan(filters, settings, scanCallback)
    }

    private val scanCallback = object : ScanCallback() {
        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            BluetoothLeScannerCompat.getScanner().stopScan(this)
            updateStatus("[LINK] TARGET FOUND: ${result.device.address}\n[LINK] INITIATING HANDSHAKE...")
            connectToDevice(result.device)
        }
        override fun onScanFailed(errorCode: Int) {
            updateError("[ERR] SCAN_FAILED_CODE_$errorCode")
        }
    }

    private fun connectToDevice(device: BluetoothDevice) {
        if (peripheral?.bluetoothDevice?.address == device.address && peripheral?.isConnected == true) {
            return
        }

        peripheral?.close()
        peripheral = MyBleManager(this).apply {
            setConnectionObserver(this@MainActivity)
            onFindPhoneRequested = { start ->
                if (getSharedPreferences("NotificationSettings", Context.MODE_PRIVATE).getBoolean("find_phone_enabled", true)) {
                    runOnUiThread { 
                        if (start) { 
                            Log.d(TAG, "Starting phone ringing")
                            ringtone?.play()
                            showFindPhoneDialog() 
                        } else { 
                            Log.d(TAG, "Stopping phone ringing")
                            ringtone?.stop() 
                            findPhoneDialog?.dismiss()
                        } 
                    }
                }
            }
            connect(device)
                .retry(5, 500)
                .useAutoConnect(true)
                .enqueue()
        }
    }

    private fun setButtonInitialState() {
        btnScan.text = "INIT_SCAN"
        ThemeHelper.applyTheme(btnScan, activeThemeColor)
    }

    private fun setButtonDisconnectedState() {
        btnScan.text = "RECONNECTING..."
        btnScan.setTextColor(ContextCompat.getColor(this, R.color.terminal_red))
        btnScan.strokeColor = ColorStateList.valueOf(ContextCompat.getColor(this, R.color.terminal_red))
    }

    override fun onDeviceConnecting(device: BluetoothDevice) {
        updateStatus("[LINK] CONNECTING TO ${device.address}...")
    }

    override fun onDeviceConnected(device: BluetoothDevice) {
        runOnUiThread {
            toolbar.subtitle = "STATUS: CONNECTED"
            updateStatus("[LINK] SECURE_CONNECTION_ESTABLISHED\n[LINK] ADDR: ${device.address}")
            layoutConnectedFeatures.visibility = View.VISIBLE
            btnScan.visibility = View.GONE
            
            getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
                .edit()
                .putString(KEY_DEVICE_ADDR, device.address)
                .apply()
                
            NotificationCatcherService.updateStatus(this, true)
        }
    }

    override fun onDeviceDisconnected(device: BluetoothDevice, reason: Int) {
        runOnUiThread {
            toolbar.subtitle = "STATUS: DISCONNECTED"
            layoutConnectedFeatures.visibility = View.GONE
            btnScan.visibility = View.VISIBLE
            setButtonDisconnectedState()

            if (reason == ConnectionObserver.REASON_LINK_LOSS) {
                updateError("[SYSTEM] LINK_LOSS_DETECTED")
                updateStatus("[SYSTEM] WAITING_FOR_REAPPEARANCE...")
            } else {
                updateStatus("[SYSTEM] CONNECTION_TERMINATED\n[SYSTEM] REASON_CODE_$reason")
            }
            
            NotificationCatcherService.updateStatus(this, false)
        }
    }

    override fun onDeviceFailedToConnect(device: BluetoothDevice, reason: Int) {
        runOnUiThread { 
            updateError("[ERR] HANDSHAKE_FAILED_$reason")
            setButtonDisconnectedState()
            if (reason == -1) {
                updateStatus("[SYSTEM] RE-INITIALIZING_STACK...")
                peripheral?.close()
            }
        }
    }
    
    override fun onDeviceReady(device: BluetoothDevice) {
        runOnUiThread { updateStatus("[SYSTEM] REMOTE_CORE_READY") }
    }
    override fun onDeviceDisconnecting(device: BluetoothDevice) {}
}
