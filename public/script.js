// Import Firebase (modular SDK)
import { initializeApp } from "https://www.gstatic.com/firebasejs/11.0.1/firebase-app.js";
import {
  getDatabase,
  ref,
  onValue
} from "https://www.gstatic.com/firebasejs/11.0.1/firebase-database.js";

// --- 🔥 Firebase Config ---
const firebaseConfig = {
  apiKey: "AIzaSyCyhipBxAUBmnBIkBPYXPKK9VH5S4lG2H4",
  authDomain: "iot-airquality-tracker.firebaseapp.com",
  databaseURL: "https://iot-airquality-tracker-default-rtdb.firebaseio.com",
  projectId: "iot-airquality-tracker",
  storageBucket: "iot-airquality-tracker.firebasestorage.app",
  messagingSenderId: "306383675937",
  appId: "1:306383675937:web:dcf49e994c01b6e68bb960"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const db = getDatabase(app);

// --- 🌍 Leaflet Map Setup ---
const map = L.map("map").setView([12.9716, 77.5946], 12);
L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
  attribution: "&copy; OpenStreetMap contributors"
}).addTo(map);

// Marker storage
let markers = {};

// --- 🧩 Helper: AQI → Color ---
function getAQIColor(aqi) {
  if (aqi <= 50) return "green";
  if (aqi <= 100) return "yellow";
  if (aqi <= 150) return "orange";
  if (aqi <= 200) return "red";
  if (aqi <= 300) return "purple";
  return "maroon";
}

// --- 🔁 Realtime Listener ---
onValue(ref(db, "SensorData"), (snapshot) => {
  const allData = snapshot.val();
  if (!allData) return;

  let latest = null;

  Object.keys(allData).forEach((key) => {
    const data = allData[key];
    const { Latitude, Longitude, PredictedAQI, Category, Advice, DustDensity, Humidity, Temperature, Color } = data;

    // --- 🗺️ Map Marker ---
    if (Latitude && Longitude) {
      const color = getAQIColor(PredictedAQI);
      const icon = L.divIcon({
        className: "custom-marker",
        html: `<div style="background:${color};width:16px;height:16px;border-radius:50%;border:2px solid #fff;"></div>`
      });

      if (markers[key]) map.removeLayer(markers[key]);
      markers[key] = L.marker([Latitude, Longitude], { icon })
        .addTo(map)
        .bindPopup(
          `<b>Category:</b> ${Category}<br>
           <b>AQI:</b> ${PredictedAQI.toFixed(1)}<br>
           <b>Temp:</b> ${Temperature} °C<br>
           <b>Dust:</b> ${DustDensity} µg/m³<br>
           <b>Humidity:</b> ${Humidity}%<br>
           <b>Advice:</b> ${Advice}`
        );
    }

    // Pick the most recent data (for dashboard)
    latest = data;
  });

  // --- 🧾 Dashboard Update ---
  if (latest) {
    document.getElementById("temp").textContent = latest.Temperature ?? "--";
    document.getElementById("humi").textContent = latest.Humidity ?? "--";
    document.getElementById("dust").textContent = latest.DustDensity ?? "--";
    document.getElementById("aqi").textContent = latest.PredictedAQI ?? "--";
    document.getElementById("category").textContent = latest.Category ?? "--";
    document.getElementById("advice").textContent = latest.Advice ?? "--";
    document.getElementById("timestamp").textContent = new Date().toLocaleString();
  }
});
