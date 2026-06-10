import json
import sqlite3
import time
from datetime import datetime

import paho.mqtt.client as mqtt

BROKER    = "localhost"
PORT      = 1883
TOPIC     = "wifi/monitor/devices"
DB_FILE   = "devices.db"

def init_db():
    conn = sqlite3.connect(DB_FILE)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS devices (
            mac         TEXT PRIMARY KEY,
            first_seen  TEXT NOT NULL,
            last_seen   TEXT NOT NULL,
            seen_count  INTEGER DEFAULT 1,
            randomized  INTEGER DEFAULT 0
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sightings (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            mac         TEXT NOT NULL,
            rssi        INTEGER,
            type        TEXT,
            channel     INTEGER,
            ts_local    TEXT
        )
    """)
    conn.commit()
    return conn

def upsert_device(conn, mac, randomized):
    now = datetime.now().isoformat()
    existing = conn.execute(
        "SELECT first_seen FROM devices WHERE mac=?", (mac,)
    ).fetchone()
    if existing:
        conn.execute(
            "UPDATE devices SET last_seen=?, seen_count=seen_count+1 WHERE mac=?",
            (now, mac)
        )
        is_new = False
    else:
        conn.execute(
            "INSERT INTO devices (mac, first_seen, last_seen, randomized) VALUES (?,?,?,?)",
            (mac, now, now, int(randomized))
        )
        is_new = True
    conn.commit()
    return is_new

conn = init_db()

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[MQTT] Connected to broker")
        client.subscribe(TOPIC)
        print(f"[MQTT] Subscribed to {TOPIC}\n")
    else:
        print(f"[MQTT] Failed to connect (rc={rc})")

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
    except json.JSONDecodeError:
        print(f"[ERROR] Bad payload: {msg.payload}")
        return

    mac        = data.get("mac", "??")
    rssi       = data.get("rssi", 0)
    frame_type = data.get("type", "?")
    randomized = data.get("randomized", False)
    channel    = data.get("channel", "?")

    is_new = upsert_device(conn, mac, randomized)

    conn.execute(
        "INSERT INTO sightings (mac, rssi, type, channel, ts_local) VALUES (?,?,?,?,?)",
        (mac, rssi, frame_type, channel, datetime.now().isoformat())
    )
    conn.commit()

    rand_tag = "(randomized)" if randomized else ""
    tag = "*** NEW DEVICE ***" if is_new else "seen"
    print(f"[{tag}] {mac} {rand_tag} | type={frame_type} | ch={channel} | rssi={rssi}dBm")

def main():
    print("=== Wi-Fi Monitor — PC Subscriber ===\n")
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    try:
        client.connect(BROKER, PORT, keepalive=60)
    except ConnectionRefusedError:
        print("[ERROR] Could not connect to MQTT broker. Is Mosquitto running?")
        return
    print("Listening... Press Ctrl+C to stop.\n")
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        conn.close()

if __name__ == "__main__":
    main()