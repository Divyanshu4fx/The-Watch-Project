package com.divya.watchappnordic.ui

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.provider.Settings
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.divya.watchappnordic.R
import com.divya.watchappnordic.util.ThemeHelper
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.switchmaterial.SwitchMaterial

class NotificationSettingsActivity : AppCompatActivity() {

    private lateinit var adapter: AppAdapter
    private val installedApps = mutableListOf<AppInfo>()
    private var activeThemeColor: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        activeThemeColor = ThemeHelper.getThemeColor(this)
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_notification_settings)

        val toolbar = findViewById<MaterialToolbar>(R.id.toolbar)
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        // Apply theme immediately
        ThemeHelper.applyThemeToActivity(this)

        val switchForwarding = findViewById<SwitchMaterial>(R.id.switchForwarding)
        val switchDnd = findViewById<SwitchMaterial>(R.id.switchDnd)
        val switchFindPhone = findViewById<SwitchMaterial>(R.id.switchFindPhone)
        val btnPermission = findViewById<Button>(R.id.btnPermission)
        val rvApps = findViewById<RecyclerView>(R.id.rvApps)

        val prefs = getSharedPreferences("NotificationSettings", Context.MODE_PRIVATE)
        
        switchForwarding.isChecked = prefs.getBoolean("forwarding_enabled", true)
        switchDnd.isChecked = prefs.getBoolean("dnd_enabled", false)
        switchFindPhone.isChecked = prefs.getBoolean("find_phone_enabled", true)

        switchForwarding.setOnCheckedChangeListener { _, isChecked ->
            prefs.edit().putBoolean("forwarding_enabled", isChecked).apply()
        }

        switchDnd.setOnCheckedChangeListener { _, isChecked ->
            prefs.edit().putBoolean("dnd_enabled", isChecked).apply()
        }

        switchFindPhone.setOnCheckedChangeListener { _, isChecked ->
            prefs.edit().putBoolean("find_phone_enabled", isChecked).apply()
        }

        btnPermission.setOnClickListener {
            startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
        }

        loadInstalledApps()
        adapter = AppAdapter(installedApps, prefs)
        rvApps.layoutManager = LinearLayoutManager(this)
        rvApps.adapter = adapter
    }

    private fun loadInstalledApps() {
        val pm = packageManager
        val packages = pm.getInstalledApplications(PackageManager.GET_META_DATA)
        for (packageInfo in packages) {
            if (pm.getLaunchIntentForPackage(packageInfo.packageName) != null) {
                installedApps.add(AppInfo(
                    pm.getApplicationLabel(packageInfo).toString(),
                    packageInfo.packageName,
                    packageInfo.loadIcon(pm)
                ))
            }
        }
        installedApps.sortBy { it.name }
    }

    override fun onSupportNavigateUp(): Boolean {
        finish()
        return true
    }

    data class AppInfo(val name: String, val packageName: String, val icon: android.graphics.drawable.Drawable)

    inner class AppAdapter(private val apps: List<AppInfo>, private val prefs: android.content.SharedPreferences) :
        RecyclerView.Adapter<AppAdapter.ViewHolder>() {

        inner class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
            val icon: ImageView = view.findViewById(R.id.ivAppIcon)
            val name: TextView = view.findViewById(R.id.tvAppName)
            val switch: SwitchMaterial = view.findViewById(R.id.switchApp)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val view = LayoutInflater.from(parent.context).inflate(R.layout.item_app_filter, parent, false)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val app = apps[position]
            holder.name.text = app.name
            holder.icon.setImageDrawable(app.icon)
            // Tag the icon to prevent theme tinting
            holder.icon.tag = ThemeHelper.TAG_KEEP_COLOR
            
            ThemeHelper.applyTheme(holder.itemView, activeThemeColor)

            val allowedApps = prefs.getStringSet("allowed_apps", mutableSetOf()) ?: mutableSetOf()
            holder.switch.isChecked = allowedApps.contains(app.packageName)

            holder.switch.setOnCheckedChangeListener { _, isChecked ->
                val currentSet = prefs.getStringSet("allowed_apps", mutableSetOf())?.toMutableSet() ?: mutableSetOf()
                if (isChecked) currentSet.add(app.packageName) else currentSet.remove(app.packageName)
                prefs.edit().putStringSet("allowed_apps", currentSet).apply()
            }
        }

        override fun getItemCount() = apps.size
    }
}
