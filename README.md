<div align="center">

# Smart Maquete

**Wi-Fi lighting controller for architectural scale models: ESP32-S3, 8 PWM channels, scenes and a phone-friendly web UI. No app and no cloud.**

![ESP32-S3](https://img.shields.io/badge/ESP32--S3-Arduino-E7352C?logo=espressif&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-firmware-00599C?logo=cplusplus&logoColor=white)
![PWM](https://img.shields.io/badge/PWM-8%20channels%20·%2010--bit-orange)
![Status](https://img.shields.io/badge/status-firmware%20v1%20done%20·%20hardware%20build%20in%20progress-yellow)
![License](https://img.shields.io/badge/license-MIT-blue)

</div>

---

Real-estate developers use illuminated scale models (*maquetes*) at sales stands. They're usually wired to a row of manual switches. **Smart Maquete** replaces the switches with an ESP32-S3 that creates its own Wi-Fi network: a broker or a visitor connects with a phone, opens a page and controls each lighting zone (façade, interior, garden, pool, each floor) with dimming, saved scenes and an automatic "tour" mode.

## Features

**Firmware v1** (`firmware/SmartMaquete`), working and tested:

- 8 independent PWM channels (LEDC, 5 kHz, 10-bit) driving logic-level MOSFETs
- Master dimmer applied on top of every channel
- Editable channel names
- Up to 6 saved scenes (save / load / delete)
- State and scenes persisted in NVS (`Preferences`), so it survives power cycles
- Responsive web UI served inline from flash (no filesystem upload needed)
- Simple JSON REST API
- Runs as its own access point, so it works on a sales floor with no internet

**Web UI v2** (`web/`), the next interface, built ahead of the firmware:

- Visitor page + separate **broker admin panel**
- Zone types (`andar` / floor, `comum` / common area, `fachada` / façade) and 12-channel layout
- Preset scenes: `dia` (day), `noite` (night), `tour` (animated sweep), `destaque` (highlight)
- Real-time sync for every connected phone over **WebSocket** (`ws://<host>/ws`)
- Works offline as a preview (falls back to default channels when the board isn't reachable)

## Architecture

```
 Phone / tablet ──Wi-Fi (AP: SmartMaquete)──▶ ESP32-S3 @ 192.168.4.1
                                               ├─ GET  /             web UI
                                               ├─ GET  /api/state    JSON state
                                               ├─ POST /api/channel  set one channel
                                               ├─ POST /api/master   master dimmer
                                               ├─ POST /api/scene/*  save / load / delete
                                               └─ LEDC PWM ×8 ──▶ MOSFETs ──▶ 12 V LED zones
```

### REST API (v1)

| Method | Endpoint | Params |
|---|---|---|
| `GET`  | `/api/state` | — (returns channels, names, master, scenes) |
| `POST` | `/api/channel` | `id`, `value` (0–1023) |
| `POST` | `/api/master` | `value` |
| `POST` | `/api/rename` | `id`, `name` (max 24 chars) |
| `POST` | `/api/scene/save` | `id`, `name` |
| `POST` | `/api/scene/load` · `/api/scene/delete` | `id` |
| `POST` | `/api/allon` · `/api/alloff` | — |

### WebSocket protocol (v2 UI)

```js
// client → board
{ action: "setChannel", channel: 0, value: 800, fade: true }
{ action: "scene", name: "noite" }          // dia | noite | tour | destaque
{ action: "renameChannel", channel: 0, name: "Cobertura", kind: "andar" }
{ action: "setTourSpeed", speed: 600 }

// board → every client (broadcast)
{ "type": "state", "channels": [{ "id": 0, "name": "Andar 1", "type": "andar", "value": 750 }],
  "scene": "noite", "tourSpeed": 800 }
```

Full protocol notes (PT-BR): [`web/PROTOCOL.pt-BR.md`](web/PROTOCOL.pt-BR.md)

## Hardware

ESP32-S3 + 8× IRLB8721 logic-level MOSFETs + LM2596 (12 V → 5 V) + Mean Well LRS-100-12. Pin map, BOM and wiring: **[docs/hardware.md](docs/hardware.md)**.

## Getting started

1. Install **Arduino IDE 2** and the **esp32 by Espressif** board package.
2. Open `firmware/SmartMaquete/SmartMaquete.ino`, select *ESP32S3 Dev Module*, upload.
3. On your phone, join Wi-Fi **`SmartMaquete`** and open **http://192.168.4.1**.

Preview the v2 UI without hardware:

```bash
cd web
python -m http.server 8080      # http://localhost:8080  and  /admin.html
```

## Security notes

This is a local, offline device, but it's still a network device, so:

- **Change the defaults before deploying.** The AP password (`maquete123`) and the v2 admin password are demo values.
- The v2 admin password is currently checked in the browser, so it's a UI gate, **not** authentication. The roadmap moves auth to the firmware (session token over the WebSocket).
- The AP uses WPA2; the API has no auth in v1, so anyone on the AP can control the lights. That's acceptable for a sales stand and not for anything else.

## Roadmap

- [ ] Firmware v2: AsyncWebServer + WebSocket broadcast + LittleFS-served UI
- [ ] Server-side admin auth for the broker panel
- [ ] Fades and the animated `tour` scene in firmware
- [ ] 12-channel board revision (custom PCB instead of perfboard)
- [ ] OTA updates

---

<sub>Built by **Nicolas Di Prospero** · IoT & security · [github.com/nicodiprospero](https://github.com/nicodiprospero)</sub>
