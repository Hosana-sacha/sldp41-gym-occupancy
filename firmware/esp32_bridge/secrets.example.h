// Copy this file to secrets.h (same folder) and fill in the values.
// secrets.h is listed in .gitignore - never commit it.
#pragma once

#define WIFI_SSID         "your-wifi-or-phone-hotspot"
#define WIFI_PASSWORD     "your-wifi-password"

// Firebase console > Project settings > General > Web API key
#define FIREBASE_API_KEY  "AIzaSyAyQqX3tjfCX3ZyFUIm1cEEZmhakbjJ3uA"

// Realtime Database host WITHOUT https:// and WITHOUT trailing slash
#define FIREBASE_DB_HOST  "sldp41-gym-occupancy-default-rtdb.firebaseio.com"

// The dedicated device account created in Firebase > Authentication.
#define DEVICE_EMAIL      "gym-device@sldp41.example.com"
#define DEVICE_PASSWORD   "FILL_ME"
