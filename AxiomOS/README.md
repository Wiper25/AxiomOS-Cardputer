# AxiomOS Cardputer — voice-only

Hold **V** → record → release (+0.7s) → ASR → Ollama → ElevenLabs → speaker.

## WiFi

1. Boot: NVS saved → else `config.h` SSID → else **WiFi setup UI**
2. Idle: **W** (or **1**) → scan → `;`/`.` select → Enter → password → Enter
3. Saved to NVS for next boot. `` ` `` = back

Optional bake-in (`src/core/config.h` / `platformio.ini`):
`AXIOM_WIFI_SSID`, `AXIOM_WIFI_PASS`, `AXIOM_VOICE_HOST`, `AXIOM_VOICE_PORT`

## Build / flash

```powershell
cd AxiomOS
py -3.12 -m platformio run -e m5stack-stamps3 -t upload --upload-port COMx
```

## Server

`Wiper25/AxiomOS` — `ws://HOST:8090/voice`. Status `pcm play` = TTS playing.
