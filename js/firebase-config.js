// Public web configuration (safe to publish: access is controlled by the
// database rules in firebase/database.rules.json, not by hiding these values).
export const firebaseConfig = {
  apiKey: "AIzaSyAyQqX3tjfCX3ZyFUIm1cEEZmhakbjJ3uA",
  authDomain: "sldp41-gym-occupancy.firebaseapp.com",
  databaseURL: "https://sldp41-gym-occupancy-default-rtdb.firebaseio.com",
  projectId: "sldp41-gym-occupancy",
  appId: "1:108904985292:web:6391305b55e2e021a5c869"
};

// Gym capacity used for the occupancy rate: rate = count / CAPACITY * 100.
// The device sends its own capacity too; this is only the fallback.
export const CAPACITY = 25;

// If the device has not written anything for this long, show "sensor offline".
// The device sends a heartbeat every 30 s, so 2 minutes tolerates a few misses.
export const STALE_AFTER_MS = 2 * 60 * 1000;

// Same thresholds as the LED on the device (Component Testing Plan):
// green <= 10, yellow 11-20, red > 20.
export const LEVELS = [
  { max: 10, key: "low", label: "Low" },
  { max: 20, key: "moderate", label: "Moderate" },
  { max: Infinity, key: "busy", label: "Busy" }
];
