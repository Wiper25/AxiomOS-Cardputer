# AxiomOS Voice Audio Terminal (firmware)

Voice **client** for M5Stack Cardputer ADV (ES8311): energy VAD / Enter wake, stream mic PCM over WebSocket, play reply PCM on the speaker.

## Firmware

Build flag: `-DAXIOM_VOICE=1` (default in `platformio.ini`).

Menu: **Сеть → Голос** (host / port / path)

| Key | Action |
|-----|--------|
| **`V`** (any screen) | Start listen / cancel (global hotkey) |
| **`Fn+V`** | Same while typing in text fields |
| Enter on Host/Port/Path | Edit settings |
| Enter on En | Toggle enable |
| Esc on voice screen | Back / cancel |

`V` auto-enables voice if off, then opens mic session (needs WiFi + server).

Default endpoint: `ws://HOST:8090/voice` — set Host in voice settings.

## Protocol

- **URL:** `ws://HOST:PORT/voice`
- **Uplink BIN:** `int16` LE mono @ **16000 Hz**, **512 samples** (1024 bytes) / frame
- **Uplink TEXT:** `{"event":"listening"|"end"|"cancel"}`
- **Downlink BIN:** same PCM format

## Server (separate repo)

Backend lives **outside** this firmware tree:

`../AxiomOS-Voice-Server` (sibling of this project)

```bash
cd ../AxiomOS-Voice-Server
pip install -r requirements.txt
python server.py --host 0.0.0.0 --port 8090
# or: docker build -t axiom-voice-server . && docker run -p 8090:8090 axiom-voice-server
```

## Device architecture

- `voice_mic` @ core0 — VAD + capture → TX queue  
- `voice_spk` @ core1 — RX queue → `M5.Speaker.playRaw`  
- `VoiceAssistant::Tick()` — WS + FSM  

States: `Idle → Listen → Thinking → Speak → Idle`.

**Perf note:** background mic/VAD is off. Configure server in **Сеть → Голос**, then press **`V`** from any screen to talk (again **`V`** to cancel).
