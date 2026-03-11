package com.divya.watchappnordic.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
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
    private val lastNotifTime = mutableMapOf<String, Long>()
    private val DEBOUNCE_MS = 300L

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        val notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Watch Notification Service")
            .setContentText("Forwarding notifications to your watch")
            .setSmallIcon(R.mipmap.ic_launcher)
            .build()
        startForeground(1, notification)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        return START_STICKY
    }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val prefs = getSharedPreferences("NotificationSettings", Context.MODE_PRIVATE)
        
        // Global toggle
        if (!prefs.getBoolean("forwarding_enabled", true)) return
        
        // DND check
        if (prefs.getBoolean("dnd_enabled", false)) return

        // App filter check
        val allowedApps = prefs.getStringSet("allowed_apps", setOf("com.whatsapp", "com.facebook.orca", "com.google.android.apps.messaging")) ?: emptySet()
        if (!allowedApps.contains(sbn.packageName)) return

        // Debounce
        val now = System.currentTimeMillis()
        val lastTime = lastNotifTime[sbn.packageName] ?: 0L
        if (now - lastTime < DEBOUNCE_MS) return
        lastNotifTime[sbn.packageName] = now

        val parsed = NotificationParser.parse(this, sbn) ?: return
        
        Log.d(TAG, "Forwarding: ${parsed.appName} | ${parsed.title}")
        
        MainActivity.peripheral?.sendNotification(parsed.appName, parsed.title, parsed.body)
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val serviceChannel = NotificationChannel(
                CHANNEL_ID,
                "Notification Service Channel",
                NotificationManager.IMPORTANCE_LOW
            )
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(serviceChannel)
        }
    }
}
