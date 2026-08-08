# AxiomOS Cardputer — voice-only

Hold **V** → record → release (+2s) → ASR → Ollama → ElevenLabs → speaker.

## Configure

Edit `src/core/config.h` or `platformio.ini` `build_flags`:

- `AXIOM_WIFI_SSID` / `AXIOM_WIFI_PASS`
- `AXIOM_VOICE_HOST` (VPS IP)
- `AXIOM_VOICE_PORT` (8090)

## Build / flash

```powershell
cd AxiomOS
py -3.12 -m platformio run -e m5stack-stamps3 -t upload --upload-port COMx
```

## Server

Repo `Wiper25/AxiomOS` — `ws://HOST:8090/voice`. On device, `pcm play` = TTS playing.
