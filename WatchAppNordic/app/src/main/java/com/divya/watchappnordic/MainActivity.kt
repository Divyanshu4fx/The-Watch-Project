package com.divya.watchappnordic

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.content.pm.PackageManager
import android.health.connect.datatypes.Device
import android.os.Bundle
import android.os.Build
import android.util.Log
import android.widget.Button
import android.widget.TextView
import androidx.annotation.RequiresApi
import androidx.annotation.RequiresPermission
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import kotlinx.coroutines.delay
import no.nordicsemi.android.support.v18.scanner.*
import no.nordicsemi.android.ble.observer.ConnectionObserver
import java.time.Instant

class MainActivity : AppCompatActivity(), ConnectionObserver {

    private lateinit var tvStatus: TextView
    private lateinit var btnScan: Button
    private lateinit var btnTimeSync: Button
    private val TAG = "BLE_SCANNER"

    private var peripheral: MyBleManager? = null

    @RequiresApi(Build.VERSION_CODES.O)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        tvStatus = findViewById(R.id.tvStatus)
        btnScan = findViewById(R.id.btnScan)
        btnTimeSync = findViewById(R.id.btnTimeSync)

        btnScan.setOnClickListener {
            Log.d(TAG, "Scan button clicked")
            if (hasPermission()) {
                startBleScan()
            } else {
                requestBlePermission()
            }
        }

        btnTimeSync.setOnClickListener {
            Log.d(TAG, "Time Sync button clicked")
            val currentUnixTimeSeconds = Instant.now().epochSecond
            peripheral?.syncTime(currentUnixTimeSeconds)
            Log.d(TAG, "Sending time: $currentUnixTimeSeconds")
        }


    }

    private fun hasPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                    ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED &&
                    ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        } else {
            ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun requestBlePermission() {
        Log.d(TAG, "Requesting permissions...")
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.ACCESS_FINE_LOCATION
            )
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        ActivityCompat.requestPermissions(this, permissions, 100)
    }

    private fun startBleScan() {
        val bluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
        if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled) {
            tvStatus.text = "Status: Bluetooth is OFF"
            Log.e(TAG, "Bluetooth is disabled")
            return
        }

        tvStatus.text = "status: Scanning (Filter: None)..."
        Log.d(TAG, "Starting scan with no filters...")

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .setReportDelay(0)
            .build()

        // Empty filters list to find ALL nearby devices
        val filters = listOf(
            ScanFilter.Builder()
                .setDeviceName("M5_Watch")
                .build()
        )

        val scanner = BluetoothLeScannerCompat.getScanner()
        scanner.startScan(filters, settings, scanCallback)
    }

    private val scanCallback = object : ScanCallback() {
        @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val deviceName = result.device.name ?: "Unknown"
            val deviceAddress = result.device.address
            val rssi = result.rssi

            Log.d(TAG, "Found: $deviceName ($deviceAddress) RSSI: $rssi")
            tvStatus.text = "Status: Found $deviceName"
            tvStatus.text = "Connecting to $deviceName"
            val scanner = BluetoothLeScannerCompat.getScanner()
            scanner.stopScan(this)

            connectToDevice(result.device)
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "Scan Failed with error code: $errorCode")
            tvStatus.text = "Status: Scan Failed ($errorCode)"
        }
    }

    override fun onStop() {
        super.onStop()
        // Stop scanning when activity is not visible to save battery
        BluetoothLeScannerCompat.getScanner().stopScan(scanCallback)
    }

    private fun connectToDevice(device: BluetoothDevice) {
        peripheral?.close()

        peripheral = MyBleManager(context = this)

        peripheral?.let { manager ->
            manager.setConnectionObserver(this)
            manager.connect(device)
                .useAutoConnect(false)
                .retry(3, 100)
                .done { connectedDevice -> Log.d(TAG, "Connected to device: ${connectedDevice?.address}")
                                           runOnUiThread { tvStatus.text = "Status Connected to ${connectedDevice?.address}" }
                 }
            .fail { failedDevice, status -> Log.e(TAG, "Connection failed for device: $status")
                                            runOnUiThread { tvStatus.text = "Status: Connection Failed $status" } }
                .enqueue()
        }

    }

    override fun onDeviceConnecting(device: BluetoothDevice) {
    Log.d(TAG, "Connecting to: ${device.address}")
    }

    override fun onDeviceConnected(device: BluetoothDevice) {
        Log.d(TAG, "Connected to: ${device.address}")
        runOnUiThread { tvStatus.text = "Status: Connected to ${device.address}(M5_Watch)"
                        btnTimeSync.isEnabled = true // Enable Button when connected
        }
    }

    override fun onDeviceFailedToConnect(device: BluetoothDevice, reason: Int) {
        Log.e("BLE_APP", "Failed to connect to: ${device.address}, Reason: $reason")
    }

    override fun onDeviceReady(device: BluetoothDevice) {
        Log.d("BLE_APP", "Device ready: ${device.address}")
    }

    override fun onDeviceDisconnecting(device: BluetoothDevice) {
        Log.d("BLE_APP", "Disconnecting from: ${device.address}")
    }

    override fun onDeviceDisconnected(device: BluetoothDevice, reason: Int) {
        Log.d("BLE_APP", "Device disconnected: ${device.address}, Reason: $reason")
        runOnUiThread { tvStatus.text = "Status: Disconnected"
                        btnTimeSync.isEnabled = false // Disable Button when disconnected
        }
    }
}
