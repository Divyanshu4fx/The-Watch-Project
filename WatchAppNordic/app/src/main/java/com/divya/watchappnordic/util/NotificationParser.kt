package com.divya.watchappnordic.util

import android.app.Notification
import android.content.Context
import android.service.notification.StatusBarNotification
import com.divya.watchappnordic.model.ParsedNotification

object NotificationParser {
    fun parse(context: Context, sbn: StatusBarNotification): ParsedNotification? {
        val notification = sbn.notification
        val extras = notification.extras

        // Filter out ongoing/foreground notifications (like media players)
        if (notification.flags and Notification.FLAG_ONGOING_EVENT != 0) return null
        
        // Filter out group summary notifications
        if (notification.flags and Notification.FLAG_GROUP_SUMMARY != 0) return null

        val packageName = sbn.packageName
        val appName = try {
            val pm = context.packageManager
            val ai = pm.getApplicationInfo(packageName, 0)
            pm.getApplicationLabel(ai).toString()
        } catch (e: Exception) {
            packageName
        }

        val title = extras.getCharSequence(Notification.EXTRA_TITLE)?.toString() ?: ""
        val body = extras.getCharSequence(Notification.EXTRA_TEXT)?.toString() 
            ?: extras.getCharSequence(Notification.EXTRA_BIG_TEXT)?.toString() ?: ""

        if (title.isEmpty() && body.isEmpty()) return null

        return ParsedNotification(appName, title, body, packageName)
    }
}
