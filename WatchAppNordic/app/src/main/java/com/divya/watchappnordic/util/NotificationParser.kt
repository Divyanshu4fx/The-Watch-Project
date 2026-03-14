package com.divya.watchappnordic.util

import android.app.Notification
import android.content.Context
import android.service.notification.StatusBarNotification
import android.util.Log
import com.divya.watchappnordic.model.ParsedNotification

object NotificationParser {
    private const val TAG = "NotifParser"

    fun parse(context: Context, sbn: StatusBarNotification): ParsedNotification? {
        val notification = sbn.notification
        val extras = notification.extras

        Log.d(TAG, "--- Analyzing Notification ---")
        Log.d(TAG, "Package: ${sbn.packageName}")
        
        // Group Summary Filter - usually safe to skip as it's a "header" notification
        if (notification.flags and Notification.FLAG_GROUP_SUMMARY != 0) {
            Log.d(TAG, "Skipped: Group Summary")
            return null
        }

        val packageName = sbn.packageName
        val appName = try {
            val pm = context.packageManager
            val ai = pm.getApplicationInfo(packageName, 0)
            pm.getApplicationLabel(ai).toString()
        } catch (e: Exception) {
            packageName
        }

        // Try to get title using multiple methods
        var title = ""
        val titleKeys = arrayOf(Notification.EXTRA_TITLE, Notification.EXTRA_TITLE_BIG, "android.title")
        for (key in titleKeys) {
            val value = extras.get(key)
            if (value != null && value.toString().isNotEmpty()) {
                title = value.toString()
                Log.d(TAG, "Found Title in '$key': $title")
                break
            }
        }

        // Try to get body using multiple methods
        var body = ""
        val bodyKeys = arrayOf(
            Notification.EXTRA_TEXT, 
            Notification.EXTRA_BIG_TEXT, 
            Notification.EXTRA_SUMMARY_TEXT, 
            Notification.EXTRA_INFO_TEXT,
            "android.text"
        )
        for (key in bodyKeys) {
            val value = extras.get(key)
            if (value != null && value.toString().isNotEmpty()) {
                body = value.toString()
                Log.d(TAG, "Found Body in '$key': $body")
                break
            }
        }

        // Final check
        if (title.isEmpty() && body.isEmpty()) {
            Log.d(TAG, "Parse FAILED. Available Keys: ${extras.keySet().joinToString(", ")}")
            // Last resort: print values of android.title and android.text if they exist
            Log.d(TAG, "Value of android.title: ${extras.get("android.title")}")
            Log.d(TAG, "Value of android.text: ${extras.get("android.text")}")
            return null
        }

        Log.d(TAG, "SUCCESS: [$appName] $title: $body")
        return ParsedNotification(appName, title, body, packageName)
    }
}
