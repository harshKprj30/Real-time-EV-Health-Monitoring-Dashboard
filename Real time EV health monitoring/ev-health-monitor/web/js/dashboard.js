/**
 * EV Health Monitor — Dashboard Logic
 * Author: harshKprj30
 *
 * Setup:
 *   1. Replace the firebaseConfig values below with your project's config.
 *      Firebase console → Project Settings → Your apps → SDK setup
 *   2. Open index.html in a browser (or deploy to Firebase Hosting).
 */

// ── Firebase Configuration ────────────────────────────
const firebaseConfig = {
  apiKey:            "YOUR_API_KEY",
  authDomain:        "YOUR_PROJECT.firebaseapp.com",
  databaseURL:       "https://YOUR_PROJECT-default-rtdb.firebaseio.com",
  projectId:         "YOUR_PROJECT_ID",
  storageBucket:     "YOUR_PROJECT.appspot.com",
  messagingSenderId: "YOUR_SENDER_ID",
  appId:             "YOUR_APP_ID"
};

firebase.initializeApp(firebaseConfig);
const evRef    = firebase.database().ref("/ev");
const alertRef = firebase.database().ref("/ev/alerts");

// ── Voltage History Chart ─────────────────────────────
const HISTORY_LEN = 25;
const voltHistory  = new Array(HISTORY_LEN).fill(395);

const chart = new Chart(document.getElementById("voltChart"), {
  type: "line",
  data: {
    labels: voltHistory.map(() => ""),
    datasets: [{
      data: [...voltHistory],
      borderColor:     "#1D9E75",
      borderWidth:     2,
      pointRadius:     0,
      tension:         0.4,
      fill:            true,
      backgroundColor: "rgba(29,158,117,0.1)"
    }]
  },
  options: {
    responsive:           true,
    maintainAspectRatio:  false,
    plugins: { legend: { display: false }, tooltip: { enabled: false } },
    scales: {
      x: { display: false },
      y: {
        min: 380, max: 420,
        ticks:  { color: "#94a3b8", callback: v => v + "V", maxTicksLimit: 4 },
        grid:   { color: "rgba(255,255,255,0.05)" },
        border: { display: false }
      }
    },
    animation: { duration: 400 }
  }
});

// ── Real-time Firebase Listener ───────────────────────
evRef.on("value", snapshot => {
  const d = snapshot.val();
  if (!d) return;

  const soc      = Math.round(d.soc      ?? 0);
  const voltage  = Math.round(d.voltage  ?? 0);
  const current  = +(d.current  ?? 0).toFixed(1);
  const power    = +(d.power    ?? 0).toFixed(1);
  const motorT   = Math.round(d.motorTemp ?? 0);
  const range    = Math.round(d.range    ?? 0);
  const rssi     = d.rssi   ?? "--";
  const uptime   = d.uptime ?? "--";

  // KPI values
  document.getElementById("soc").textContent     = soc;
  document.getElementById("voltage").textContent = voltage;
  document.getElementById("current").textContent = current;
  document.getElementById("mtemp").textContent   = motorT;
  document.getElementById("rssi").textContent    = rssi;
  document.getElementById("range").textContent   = "Range: " + range + " km";
  document.getElementById("power").textContent   = "Power: " + power + " kW";
  document.getElementById("mstatus").textContent = motorT > 80 ? "⚠ High — check motor" : "Within normal range";
  document.getElementById("uptime-val").textContent = "Uptime: " + uptime;
  document.getElementById("ts").textContent      = new Date().toLocaleTimeString();

  // SoC progress bar colour
  const socBar = document.getElementById("soc-bar");
  socBar.style.width      = soc + "%";
  socBar.style.background = soc > 50 ? "#1d9e75" : soc > 20 ? "#ef9f27" : "#e24b4a";

  // Voltage trend chart
  voltHistory.push(voltage);
  voltHistory.shift();
  chart.data.datasets[0].data = [...voltHistory];
  chart.update("none");

  // Thermal sensors
  renderThermal(d);
});

// ── Thermal Sensors Renderer ──────────────────────────
function renderThermal(d) {
  const sensors = [
    { label: "Battery pack",  value: d.battTemp,  max: 60,  unit: "°C" },
    { label: "Motor",         value: d.motorTemp, max: 120, unit: "°C" },
    { label: "Ambient",       value: 32,          max: 50,  unit: "°C" },
  ];

  document.getElementById("thermal-sensors").innerHTML = sensors.map(s => {
    const val  = +(s.value ?? 0).toFixed(1);
    const pct  = Math.min(100, (val / s.max) * 100).toFixed(1);
    const color = pct > 83 ? "#e24b4a" : pct > 66 ? "#ef9f27" : "#1d9e75";
    return `
      <div class="sensor-row">
        <span class="sensor-lbl">${s.label}</span>
        <div class="sensor-bar-bg">
          <div class="sensor-bar-fill" style="width:${pct}%;background:${color}"></div>
        </div>
        <span class="sensor-val" style="color:${color}">${val}${s.unit}</span>
      </div>`;
  }).join("");
}

// ── Alert Log Listener ────────────────────────────────
alertRef.on("value", snapshot => {
  const alerts = snapshot.val();
  const container = document.getElementById("alert-log");

  if (!alerts || Object.keys(alerts).length === 0) {
    container.innerHTML = '<p class="no-alerts">No active alerts — all systems normal ✓</p>';
    return;
  }

  const now = new Date().toLocaleTimeString();
  container.innerHTML = Object.entries(alerts).map(([key, msg]) => {
    const isWarn = String(msg).toLowerCase().includes("high");
    return `
      <div class="alert-item ${isWarn ? "" : "warn"}">
        <span class="alert-icon">${isWarn ? "🔴" : "🟡"}</span>
        <div>
          <div class="alert-text">${key}: ${msg}</div>
          <div class="alert-time">Detected at ${now}</div>
        </div>
      </div>`;
  }).join("");
});
