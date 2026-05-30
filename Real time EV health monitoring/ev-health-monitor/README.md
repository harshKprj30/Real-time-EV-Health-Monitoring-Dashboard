# ⚡ Real-time EV Health Monitoring Dashboard

An IoT-based system for monitoring Electric Vehicle health metrics in real-time using NodeMCU/Raspberry Pi, Firebase, and a live web dashboard.

![Dashboard Preview](https://img.shields.io/badge/Status-Live-brightgreen) ![Platform](https://img.shields.io/badge/Platform-NodeMCU%20%7C%20RPi-blue) ![Cloud](https://img.shields.io/badge/Cloud-Firebase-orange)

---

## 🏗️ System Architecture

```
[EV Sensors] → [NodeMCU/RPi] → [Firebase RTDB] → [Web Dashboard]
   Voltage          ADC           HTTPS/MQTT        Real-time UI
   Current        ESP8266          Cloud DB          Charts
   Temp            WiFi           Auth + Rules       Alerts
```

---

## 📦 Features

- 🔋 Real-time State of Charge (SoC) with estimated range
- ⚡ Pack voltage & current draw monitoring
- 🌡️ Multi-zone thermal sensors (battery, motor, inverter, ambient)
- 📊 Rolling voltage trend chart (Chart.js)
- 🔬 Per-cell voltage health grid (green/amber/red)
- 📡 NodeMCU/RPi node status — WiFi RSSI, uptime, firmware
- 🚨 Event log with BMS alerts and temperature spike detection
- 🔔 Firebase Cloud Messaging push notifications

---

## 🛒 Hardware Requirements

| Component | Purpose | Approx. Cost |
|---|---|---|
| NodeMCU ESP8266 | Main microcontroller + WiFi | ₹250 |
| INA219 (I2C) | Voltage + current sensor | ₹150 |
| DS18B20 | Temperature sensor (1-Wire) | ₹80 |
| ACS712 | High-current AC/DC sensor | ₹120 |
| NTC Thermistor | Ambient temperature | ₹30 |
| Breadboard + jumpers | Prototyping | ₹100 |

---

## 🔌 Wiring Diagram

```
NodeMCU       INA219 (I2C)
D1   -------> SCL
D2   -------> SDA
3.3V -------> VCC
GND  -------> GND

NodeMCU       DS18B20 (1-Wire)
D4   -------> DATA (with 4.7kΩ pull-up to 3.3V)
3.3V -------> VCC
GND  -------> GND
```

---

## 🚀 Setup Instructions

### 1. Firebase Setup
1. Go to [console.firebase.google.com](https://console.firebase.google.com)
2. Create a new project → Enable **Realtime Database**
3. Copy your project credentials into `arduino/ev_monitor.ino`
4. Set database rules from `firebase/database.rules.json`

### 2. NodeMCU Firmware
1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP8266 board: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Install libraries: `FirebaseESP8266`, `INA219`, `DallasTemperature`, `OneWire`
4. Open `arduino/ev_monitor.ino`, fill in your WiFi + Firebase credentials
5. Flash to NodeMCU

### 3. Web Dashboard
1. Open `web/index.html` in a browser, **or**
2. Deploy to Firebase Hosting:
```bash
npm install -g firebase-tools
firebase login
firebase init hosting
firebase deploy
```

### 4. Raspberry Pi (Alternative)
```bash
pip install firebase-admin smbus2
python python/ev_monitor.py
```

---

## 📁 Project Structure

```
ev-dashboard/
├── arduino/
│   └── ev_monitor.ino       # NodeMCU firmware
├── python/
│   └── ev_monitor.py        # Raspberry Pi script
├── web/
│   ├── index.html           # Dashboard UI
│   ├── css/style.css        # Styles
│   └── js/dashboard.js      # Firebase + Chart.js logic
├── firebase/
│   └── database.rules.json  # Security rules
└── README.md
```

---

## 🔔 Alert Thresholds

| Parameter | Warning | Critical |
|---|---|---|
| Battery Temp | > 40°C | > 50°C |
| Motor Temp | > 80°C | > 100°C |
| State of Charge | < 20% | < 10% |
| Cell Voltage | < 3.2V | < 2.8V |
| Pack Voltage | < 340V | < 300V |

---

## 🛠️ Tech Stack

- **Firmware**: Arduino C++ (ESP8266) / Python (RPi)
- **Cloud**: Google Firebase Realtime Database + FCM
- **Frontend**: HTML5, CSS3, JavaScript, Chart.js
- **Protocols**: HTTPS REST / MQTT

---

## 📜 License

MIT License — free to use, modify, and distribute.

---

## 👤 Author

**Harsh Kumar** — [@harshKprj30](https://github.com/harshKprj30)
