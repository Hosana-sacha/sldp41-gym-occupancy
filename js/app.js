// Live gym occupancy display.
// Listens to /gym in Firebase Realtime Database; the browser is pushed every
// change, so there is no polling and no page refresh.

import { initializeApp } from "https://www.gstatic.com/firebasejs/11.0.2/firebase-app.js";
import { getDatabase, ref, onValue } from "https://www.gstatic.com/firebasejs/11.0.2/firebase-database.js";
import { firebaseConfig, CAPACITY, STALE_AFTER_MS, LEVELS } from "./firebase-config.js";

const db = getDatabase(initializeApp(firebaseConfig));

const el = {
  card: document.querySelector(".card"),
  count: document.getElementById("count"),
  rate: document.getElementById("rate"),
  capacity: document.getElementById("capacity"),
  meter: document.getElementById("meter"),
  fill: document.getElementById("meterFill"),
  status: document.getElementById("status"),
  updated: document.getElementById("updated")
};

let latest = null;        // last value of /gym: { count, occupancyRate, capacity, updatedAt }
let connected = false;    // are we connected to Firebase right now?
let serverOffset = 0;     // (server clock - browser clock) in ms, so "x s ago" is correct
let readError = null;

// --- Subscriptions ---------------------------------------------------------
onValue(ref(db, ".info/serverTimeOffset"), (snap) => { serverOffset = snap.val() || 0; });
onValue(ref(db, ".info/connected"), (snap) => { connected = snap.val() === true; render(); });
onValue(
  ref(db, "gym"),
  (snap) => { latest = snap.val(); readError = null; render(); },
  (err) => { readError = err; render(); }
);
setInterval(render, 1000); // keeps "updated x s ago" and the offline check fresh

// --- Helpers ---------------------------------------------------------------
function levelFor(count) {
  return LEVELS.find((l) => count <= l.max);
}

function ago(ms) {
  const s = Math.max(0, Math.round(ms / 1000));
  if (s < 5) return "just now";
  if (s < 60) return s + " s ago";
  const m = Math.round(s / 60);
  if (m < 60) return m + " min ago";
  return Math.round(m / 60) + " h ago";
}

function setStatus(level, text) {
  el.status.dataset.level = level;
  el.status.textContent = text;
}

// --- Rendering -------------------------------------------------------------
function render() {
  if (readError) {
    el.card.classList.add("offline");
    el.count.textContent = "–";
    setStatus("offline", "Cannot read data");
    el.updated.textContent = "Check the database rules or try again later.";
    return;
  }

  if (!latest || !Number.isFinite(latest.count)) {
    el.card.classList.remove("offline");
    el.count.textContent = "–";
    el.rate.textContent = "–";
    el.fill.style.width = "0%";
    setStatus("unknown", connected ? "Waiting for the sensor…" : "Connecting…");
    el.updated.innerHTML = "&nbsp;";
    return;
  }

  const capacity = Number.isFinite(latest.capacity) && latest.capacity > 0 ? latest.capacity : CAPACITY;
  const count = latest.count;
  const rate = Number.isFinite(latest.occupancyRate) ? latest.occupancyRate : (count / capacity) * 100;
  const age = Number.isFinite(latest.updatedAt) ? Date.now() + serverOffset - latest.updatedAt : Infinity;
  const stale = age > STALE_AFTER_MS;

  el.count.textContent = String(count);
  el.rate.textContent = String(Math.round(rate));
  el.capacity.textContent = String(capacity);

  const pct = Math.max(0, Math.min(100, rate));
  el.fill.style.width = pct + "%";
  el.meter.setAttribute("aria-valuenow", String(Math.round(pct)));

  const level = levelFor(count);
  el.card.classList.toggle("offline", stale);

  if (stale) {
    el.fill.style.background = "var(--unknown)";
    setStatus("offline", "Sensor offline");
    el.updated.textContent = Number.isFinite(age)
      ? "Last reading " + ago(age) + " – the number above may be out of date."
      : "No timestamp from the sensor.";
  } else {
    el.fill.style.background = "var(--" + level.key + ")";
    setStatus(level.key, level.label);
    el.updated.textContent = "Updated " + ago(age) + (connected ? "" : " (reconnecting…)");
  }
}

render();
