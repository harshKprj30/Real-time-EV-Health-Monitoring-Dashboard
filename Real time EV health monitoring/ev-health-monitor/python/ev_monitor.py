#!/usr/bin/env python3
"""
Real-time EV Health Monitoring System — Raspberry Pi
Author: harshKprj30 | Version: 2.3.1

Requirements:
    pip install firebase-admin smbus2

Hardware:
    INA219  → RPi I2C bus (GPIO 2/3, addr 0x40)
    DS18B20 → GPIO 4 (1-Wire, enable in /boot/config.txt: dtoverlay=w1-gpio)

Setup:
    1. Download serviceAccountKey.json from Firebase console
    2. Replace YOUR_PROJECT in databaseURL below
    3. Run: python3 ev_monitor.py
"""

import time
import glob
import struct
import smbus2
import firebase_admin
from firebase_admin import credentials, db

# ── Firebase Setup ─────────────────────────────────────
cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred, {
    "databaseURL": "https://YOUR_PROJECT-default-rtdb.firebaseio.com"
})
ev_ref = db.reference("/ev")
alert_ref = db.reference("/ev/alerts")

# ── INA219 I2C Constants ───────────────────────────────
INA219_ADDR      = 0x40
REG_BUS_VOLTAGE  = 0x02
REG_CURRENT      = 0x04
SHUNT_RESISTANCE = 0.1   # Ohms (matches your shunt resistor)
PACK_SCALE       = 100   # Scale INA219 bus reading to full pack voltage

# ── Thresholds ────────────────────────────────────────
BATT_FULL_V   = 420.0
BATT_EMPTY_V  = 320.0
ALERT_TEMP    = 80.0     # Motor temp °C
ALERT_SOC     = 15.0     # Low SoC %
PUSH_INTERVAL = 2.0      # seconds

bus = smbus2.SMBus(1)

# ── Sensor helpers ─────────────────────────────────────
def read_ina219_voltage() -> float:
    """Return estimated pack voltage in volts."""
    raw = bus.read_word_data(INA219_ADDR, REG_BUS_VOLTAGE)
    raw = ((raw & 0xFF) << 8) | (raw >> 8)          # swap bytes
    millivolts = (raw >> 3) * 4                      # 4 mV per LSB
    return (millivolts / 1000.0) * PACK_SCALE

def read_ina219_current() -> float:
    """Return current in amps (negative = charging)."""
    raw = bus.read_word_data(INA219_ADDR, REG_CURRENT)
    raw = ((raw & 0xFF) << 8) | (raw >> 8)
    if raw > 32767:
        raw -= 65536
    return (raw * 1e-5) / SHUNT_RESISTANCE

def read_ds18b20(sensor_index: int = 0) -> float:
    """Read DS18B20 temperature via Linux 1-Wire sysfs."""
    devices = glob.glob("/sys/bus/w1/devices/28-*")
    if not devices or sensor_index >= len(devices):
        return 25.0                                  # fallback
    try:
        with open(devices[sensor_index] + "/w1_slave") as f:
            lines = f.readlines()
        if "YES" in lines[0]:
            return float(lines[1].split("t=")[1]) / 1000.0
    except (IOError, IndexError, ValueError):
        pass
    return 0.0

def estimate_soc(voltage: float) -> float:
    if voltage >= BATT_FULL_V:  return 100.0
    if voltage <= BATT_EMPTY_V: return 0.0
    return round((voltage - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100, 1)

def format_uptime(seconds: int) -> str:
    return f"{seconds // 3600}h {(seconds % 3600) // 60}m"

# ── Main loop ──────────────────────────────────────────
start_time = time.time()
print("EV Monitor started — pushing to Firebase every 2s")
print("Press Ctrl+C to stop.\n")

while True:
    loop_start = time.time()
    try:
        voltage    = read_ina219_voltage()
        current    = read_ina219_current()
        batt_temp  = read_ds18b20(0)
        motor_temp = read_ds18b20(1)
        soc        = estimate_soc(voltage)
        power_kw   = abs(voltage * current) / 1000.0
        uptime_sec = int(time.time() - start_time)

        payload = {
            "voltage":   round(voltage, 1),
            "current":   round(abs(current), 2),
            "power":     round(power_kw, 2),
            "soc":       soc,
            "range":     round(soc * 3.1, 1),
            "battTemp":  round(batt_temp, 1),
            "motorTemp": round(motor_temp, 1),
            "charging":  bool(current < 0),
            "uptime":    format_uptime(uptime_sec),
            "ts":        int(time.time()),
        }
        ev_ref.update(payload)

        # Alerts
        if motor_temp > ALERT_TEMP:
            alert_ref.child("motorTemp").set(
                f"HIGH: {motor_temp:.1f}C at {format_uptime(uptime_sec)}")
        if soc < ALERT_SOC:
            alert_ref.child("soc").set(
                f"LOW: {soc}% at {format_uptime(uptime_sec)}")

        print(f"[EV] V={voltage:.1f}V  I={current:.2f}A  "
              f"SoC={soc}%  BT={batt_temp:.1f}C  MT={motor_temp:.1f}C")

    except KeyboardInterrupt:
        print("\nStopped.")
        break
    except Exception as err:
        print(f"[ERROR] {err}")

    # Maintain consistent push interval
    elapsed = time.time() - loop_start
    time.sleep(max(0, PUSH_INTERVAL - elapsed))
