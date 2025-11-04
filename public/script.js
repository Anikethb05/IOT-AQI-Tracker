// Import Firebase modular SDK
import { initializeApp } from "https://www.gstatic.com/firebasejs/11.0.1/firebase-app.js";
import {
  getDatabase,
  ref,
  query,
  limitToLast,
  onChildAdded,
} from "https://www.gstatic.com/firebasejs/11.0.1/firebase-database.js";

// Firebase config
const firebaseConfig = {
  apiKey: "AIzaSyCyhipBxAUBmnBIkBPYXPKK9VH5S4lG2H4",
  authDomain: "iot-airquality-tracker.firebaseapp.com",
  databaseURL: "https://iot-airquality-tracker-default-rtdb.firebaseio.com",
  projectId: "iot-airquality-tracker",
  storageBucket: "iot-airquality-tracker.firebasestorage.app",
  messagingSenderId: "306383675937",
  appId: "1:306383675937:web:dcf49e994c01b6e68bb960",
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const db = getDatabase(app);
const sensorRef = query(ref(db, "SensorData"), limitToLast(1));

// Leaflet map setup
const map = L.map("map").setView([12.9716, 77.5946], 12);
L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
  attribution: "&copy; OpenStreetMap contributors",
}).addTo(map);

let marker;

// Function to get color based on AQI value
function getAQIColor(aqi) {
  if (aqi <= 50) return "#00e400"; // Green
  if (aqi <= 100) return "#ffff00"; // Yellow
  if (aqi <= 150) return "#ff7e00"; // Orange
  if (aqi <= 200) return "#ff0000"; // Red
  if (aqi <= 300) return "#8f3f97"; // Purple
  return "#7e0023"; // Maroon
}

// Listen for latest sensor data
onChildAdded(sensorRef, (snapshot) => {
  const data = snapshot.val();
  if (!data) {
    console.warn("No data received!");
    return;
  }

  console.log("Received data:", data);

  // Update dashboard
  document.getElementById("temp").textContent =
    data.Temperature?.toFixed?.(1) ?? "--";
  document.getElementById("humi").textContent =
    data.Humidity?.toFixed?.(1) ?? "--";
  document.getElementById("dust").textContent =
    data.DustDensity?.toFixed?.(2) ?? "--";
  document.getElementById("aqi").textContent =
    data.PredictedAQI?.toFixed?.(1) ?? "--";
  document.getElementById("timestamp").textContent = new Date().toLocaleString();

  // Update map marker
  if (data.Latitude && data.Longitude) {
    if (marker) map.removeLayer(marker);

    const color = getAQIColor(data.PredictedAQI ?? 0);

    const icon = L.divIcon({
      className: "aqi-marker",
      html: `<div style="background:${color}" class="aqi-marker"></div>`,
      iconSize: [20, 20],
    });

    marker = L.marker([data.Latitude, data.Longitude], { icon })
      .addTo(map)
      .bindPopup(
        `<b>Category:</b> ${data.Category}<br>
         <b>Dust Density:</b> ${data.DustDensity} µg/m³<br>
         <b>Humidity:</b> ${data.Humidity}%<br>
         <b>Predicted AQI:</b> ${data.PredictedAQI}<br>
         <b>Advice:</b> ${data.Advice}`
      )
      .openPopup();

    map.setView([data.Latitude, data.Longitude], 13);
  }
});
