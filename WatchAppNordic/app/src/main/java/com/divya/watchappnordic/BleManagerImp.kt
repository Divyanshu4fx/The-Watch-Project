package com.divya.watchappnordic

import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.content.Context
import no.nordicsemi.android.ble.BleManager
import android.util.Log
import java.util.UUID

class MyBleManager(context: Context) : BleManager(context) {
    private val tag = "BLE_MANAGER"

    companion object {
        val SERVICE_UUID: UUID = UUID.fromString("f0debc9a-7856-3412-7856-341278563412")
        val TIME_CHAR_UUID: UUID = UUID.fromString("cdabefcd-abef-cdab-cdab-efcdabefcdab")
        val ALARM_CHAR_UUID: UUID = UUID.fromString("edeec838-c010-c58a-1e45-69c476cc73e7")
        val FIND_WATCH_CHAR_UUID: UUID = UUID.fromString("3e6e06ea-8e05-7195-794b-5d3dad032e87")
        val NOTIF_CHAR_UUID: UUID = UUID.fromString("56dcbe22-a23d-51af-7940-1d1a2dbcdc15")
        val FIND_PHONE_CHAR_UUID: UUID = UUID.fromString("c568a991-7b46-20b6-f746-fbdc7310da00")
        
        // IR Remote Characteristic
        val IR_CHAR_UUID: UUID = UUID.fromString("4ff21e58-1ce1-c19a-6348-b21db24b0633")
    }

    private var timeCharacteristic: BluetoothGattCharacteristic? = null
    private var alarmCharacteristic: BluetoothGattCharacteristic? = null
    private var findWatchCharacteristic: BluetoothGattCharacteristic? = null
    private var notificationCharacteristic: BluetoothGattCharacteristic? = null
    private var findPhoneCharacteristic: BluetoothGattCharacteristic? = null
    private var irCharacteristic: BluetoothGattCharacteristic? = null

    var onAlarmReceived: ((Int, Int, Int, String, Boolean) -> Unit)? = null
    var onFindPhoneRequested: ((Boolean) -> Unit)? = null

    override fun isRequiredServiceSupported(gatt: BluetoothGatt): Boolean {
        val service = gatt.getService(SERVICE_UUID)
        if (service != null) {
            timeCharacteristic = service.getCharacteristic(TIME_CHAR_UUID)
            alarmCharacteristic = service.getCharacteristic(ALARM_CHAR_UUID)
            findWatchCharacteristic = service.getCharacteristic(FIND_WATCH_CHAR_UUID)
            findPhoneCharacteristic = service.getCharacteristic(FIND_PHONE_CHAR_UUID)
            notificationCharacteristic = service.getCharacteristic(NOTIF_CHAR_UUID)
            irCharacteristic = service.getCharacteristic(IR_CHAR_UUID)
        }
        
        Log.d(tag, "Service Discovery: Time=${timeCharacteristic != null}, Alarm=${alarmCharacteristic != null}, FindWatch=${findWatchCharacteristic != null}, Notif=${notificationCharacteristic != null}, FindPhone=${findPhoneCharacteristic != null}, IR=${irCharacteristic != null}")
        return timeCharacteristic != null
    }

    override fun initialize() {
        setNotificationCallback(alarmCharacteristic).with { _, data ->
            val bytes = data.value
            if (bytes != null && bytes.size >= 4) {
                val index = bytes[0].toInt()
                val hour = bytes[1].toInt()
                val minute = bytes[2].toInt()
                val enabled = bytes[3].toInt() == 1
                val message = if (bytes.size > 4) String(bytes.sliceArray(4 until bytes.size)) else ""
                onAlarmReceived?.invoke(index, hour, minute, message, enabled)
            }
        }
        enableNotifications(alarmCharacteristic).enqueue()

        setNotificationCallback(findPhoneCharacteristic).with { _, data ->
            val bytes = data.value
            if (bytes != null && bytes.isNotEmpty()) {
                val command = bytes[0].toInt()
                Log.d(tag, "Find My Phone BLE notification: $command")
                onFindPhoneRequested?.invoke(command == 1)
            }
        }
        enableNotifications(findPhoneCharacteristic).enqueue()
    }

    override fun onServicesInvalidated() {
        timeCharacteristic = null
        alarmCharacteristic = null
        findWatchCharacteristic = null
        notificationCharacteristic = null
        findPhoneCharacteristic = null
        irCharacteristic = null
    }

    fun syncTime(unixTimeSeconds: Long) {
        timeCharacteristic?.let { char ->
            val bytes = ByteArray(4)
            bytes[0] = (unixTimeSeconds and 0xFF).toByte()
            bytes[1] = ((unixTimeSeconds shr 8) and 0xFF).toByte()
            bytes[2] = ((unixTimeSeconds shr 16) and 0xFF).toByte()
            bytes[3] = ((unixTimeSeconds shr 24) and 0xFF).toByte()
            writeCharacteristic(char, bytes, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT).enqueue()
        }
    }

    fun findMyWatch(start: Boolean) {
        findWatchCharacteristic?.let { char ->
            val command = if (start) 1.toByte() else 0.toByte()
            writeCharacteristic(char, byteArrayOf(command), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT).enqueue()
        }
    }

    fun sendNotification(appName: String, title: String, body: String) {
        notificationCharacteristic?.let { char ->
            val payload = "$appName|$title|$body"
            val payloadBytes = payload.toByteArray(Charsets.UTF_8)
            val truncatedBytes = if (payloadBytes.size > 181) {
                payloadBytes.sliceArray(0 until 178) + "...".toByteArray()
            } else payloadBytes
            
            val data = ByteArray(1 + truncatedBytes.size)
            data[0] = 0x01.toByte()
            System.arraycopy(truncatedBytes, 0, data, 1, truncatedBytes.size)
            writeCharacteristic(char, data, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE).enqueue()
        }
    }

    fun sendIrCommand(protocol: Int, hexCode: Long) {
        irCharacteristic?.let { char ->
            val data = ByteArray(5)
            data[0] = protocol.toByte()
            data[1] = (hexCode shr 24 and 0xFF).toByte()
            data[2] = (hexCode shr 16 and 0xFF).toByte()
            data[3] = (hexCode shr 8 and 0xFF).toByte()
            data[4] = (hexCode and 0xFF).toByte()
            writeCharacteristic(char, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT).enqueue()
        }
    }

    fun requestAlarms() {
        alarmCharacteristic?.let { char ->
            writeCharacteristic(char, byteArrayOf(0xFF.toByte()), BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT).enqueue()
        }
    }

    fun setAlarm(index: Int, hour: Int, minute: Int, message: String, enabled: Boolean) {
        alarmCharacteristic?.let { char ->
            val messageBytes = message.toByteArray()
            val data = ByteArray(4 + messageBytes.size)
            data[0] = index.toByte()
            data[1] = hour.toByte()
            data[2] = minute.toByte()
            data[3] = if (enabled) 1.toByte() else 0.toByte()
            System.arraycopy(messageBytes, 0, data, 4, messageBytes.size)
            writeCharacteristic(char, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT).enqueue()
        }
    }
}
