package com.divya.watchappnordic.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.util.Log
import androidx.core.app.NotificationCompat
import com.divya.watchappnordic.MainActivity
import com.divya.watchappnordic.R
import com.divya.watchappnordic.util.NotificationParser

class NotificationCatcherService : NotificationListenerService() {

    private val TAG = "NotifService"
    private val CHANNEL_ID = "NotificationServiceChannel"
    private val NOTIF_ID = 1
    private val lastNotifTime = mutableMapOf<String, Long>()
    private val DEBOUNCE_MS = 300L

    companion object {
        fun updateStatus(context: Context, isConnected: Boolean) {
            val intent = Intent(context, NotificationCatcherService::class.java).apply {
                action = if (isConnected) "ACTION_CONNECTED" else "ACTION_DISCONNECTED"
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "NotificationCatcherService Created")
        createNotificationChannel()
        startForeground(NOTIF_ID, createStatusNotification(false))
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        intent?.action?.let { action ->
            val isConnected = action == "ACTION_CONNECTED"
            val notificationManager = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            notificationManager.notify(NOTIF_ID, createStatusNotification(isConnected))
        }
        return START_STICKY
    }

    private fun createStatusNotification(isConnected: Boolean): Notification {
        val statusText = if (isConnected) "[LINK] ACTIVE: M5_WATCH" else "[LINK] OFFLINE: WAITING..."
        val contentIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("M5_WATCH_OS")
            .setContentText(statusText)
            .setSmallIcon(R.mipmap.ic_launcher)
            .setOngoing(true)
            .setContentIntent(contentIntent)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setColor(0x41ff00) // Terminal Green
            .build()
    }

    override fun onListenerConnected() {
        super.onListenerConnected()
        Log.d(TAG, "Notification Listener Connected")
    }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val packageName = sbn.packageName
        val prefs = getSharedPreferences("NotificationSettings", Context.MODE_PRIVATE)
        
        if (!prefs.getBoolean("forwarding_enabled", true)) return
        if (prefs.getBoolean("dnd_enabled", false)) return

        val allowedApps = prefs.getStringSet("allowed_apps", emptySet()) ?: emptySet()
        if (!allowedApps.contains(packageName)) return

        val now = System.currentTimeMillis()
        val lastTime = lastNotifTime[packageName] ?: 0L
        if (now - lastTime < DEBOUNCE_MS) return
        lastNotifTime[packageName] = now

        val parsed = NotificationParser.parse(this, sbn) ?: return
        
        Log.d(TAG, "FORWARDING: [${parsed.appName}] ${parsed.title}")
        MainActivity.peripheral?.sendNotification(parsed.appName, parsed.title, parsed.body)
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val serviceChannel = NotificationChannel(
                CHANNEL_ID,
                "M5 Watch Status Service",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Persistent link status for M5_Watch"
                setShowBadge(false)
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(serviceChannel)
        }
    }
}
