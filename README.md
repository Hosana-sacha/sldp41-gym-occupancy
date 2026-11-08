# SLDP 41 – Live gym occupancy

Student project (NYU, SLDP 41). Shows how many people are in the gym, in real time.

```
2 x HC-SR04 -> Arduino UNO (counts) --serial--> ESP32 (Wi-Fi) --HTTPS--> Firebase Realtime DB --push--> this website
```

| Path | What it is |
|---|---|
| `index.html`, `css/`, `js/` | The static website (GitHub Pages). `js/app.js` listens to Firebase live. |
| `js/firebase-config.js` | Public Firebase web config + capacity (25) + LED thresholds. |
| `firebase/database.rules.json` | Security rules: anyone reads `/gym`, only the device account writes. |
| `firmware/uno_sender/` | UNO helper: `publishIfNeeded(count)` sends `count,rate` to the ESP32. |
| `firmware/esp32_bridge/` | ESP32 sketch: reads the UNO line, signs in, uploads to Firebase. |
| `assets/` | Put `nyu-logo.svg` here (see `assets/README.md`). |

Data stored at `/gym`: `count`, `occupancyRate` (= count / 25 x 100), `capacity`, `updatedAt` (server time).

Full plan, function-by-function explanation and known weak points are in the project's Google Doc.

**Setup of the ESP32:** copy `firmware/esp32_bridge/secrets.example.h` to `secrets.h`, fill it in, flash with the Arduino IDE (board: ESP32 Dev Module). `secrets.h` is git-ignored.
