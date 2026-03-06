package com.divya.watchappnordic

import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.content.Context
import no.nordicsemi.android.ble.BleManager
import android.util.Log
import java.util.UUID

class MyBleManager(context: Context) : BleManager(context)
{
    private var TAG = "BLE_MANAGER"

    companion object{
        val SERVICE_UUID = UUID.fromString("f0debc9a-7856-3412-7856-341278563412")
        val TIME_CHAR_UUID = UUID.fromString("cdabefcd-abef-cdab-cdab-efcdabefcdab")
    }

    private var timeCharacteristic: BluetoothGattCharacteristic? = null

    override fun isRequiredServiceSupported(gatt: BluetoothGatt): Boolean {
        val service = gatt.getService(SERVICE_UUID)
        if(service != null)
        {
            timeCharacteristic = service.getCharacteristic(TIME_CHAR_UUID)
        }

        Log.d(TAG,"Required service status: ${timeCharacteristic != null}")
        return timeCharacteristic != null
    }
    override fun onServicesInvalidated() {
        Log.d(TAG, "onServicesInvalidated called")
        timeCharacteristic = null
    }

    fun syncTime(unixTimeSeconds: Long)
    {
        timeCharacteristic?.let{ char ->
            // Convert Long to 4-byte array (Little Endian to match ESP32)
            val bytes = ByteArray(4)
            bytes[0] = (unixTimeSeconds and 0xFF).toByte()
            bytes[1] = ((unixTimeSeconds shr 8) and 0xFF).toByte()
            bytes[2] = ((unixTimeSeconds shr 16) and 0xFF).toByte()
            bytes[3] = ((unixTimeSeconds shr 24) and 0xFF).toByte()

            writeCharacteristic(char, bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT).enqueue()
        } ?: run {
            Log.e(TAG, "Time characteristic is not found or device not connected")

    }
    }
}