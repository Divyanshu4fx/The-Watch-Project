package com.divya.watchappnordic.model

data class ParsedNotification(
    val appName: String,
    val title: String,
    val body: String,
    val packageName: String
)
