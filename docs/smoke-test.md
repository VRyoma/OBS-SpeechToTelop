# SpeechToTelop — Smoke Test Checklist

Run these steps manually after building and installing the plugin.

## Prerequisites

- OBS Studio installed (v29+ recommended)
- Plugin built and copied to OBS plugins directory
- Microphone available

## Install (macOS)

```bash
cmake --build build --config RelWithDebInfo
cp build/SpeechToTelop.dylib \
  ~/Library/Application\ Support/obs-studio/plugins/SpeechToTelop/bin/SpeechToTelop.dylib
cp -r data/ \
  ~/Library/Application\ Support/obs-studio/plugins/SpeechToTelop/data/
```

## Install (Windows)

```bat
cmake --build build --config RelWithDebInfo
copy build\RelWithDebInfo\SpeechToTelop.dll ^
  "%APPDATA%\obs-studio\plugins\SpeechToTelop\bin\64bit\SpeechToTelop.dll"
xcopy /E /I data ^
  "%APPDATA%\obs-studio\plugins\SpeechToTelop\data"
```

## Test Steps

### Step 1: Plugin loads
- [ ] Open OBS Studio
- [ ] No crash on startup
- [ ] Check Help → About or OBS log: `speech_to_telop_filter` and `speech_to_telop_source` appear

### Step 2: Add audio filter
- [ ] In Audio Mixer, right-click microphone → Filters
- [ ] Click "+" → "Speech to Telop Filter" appears in the list
- [ ] Add it; properties dialog opens
- [ ] Select Model: **base**; Backend: **Local (whisper.cpp)**
- [ ] Close filters
- [ ] Check OBS log: "モデルをダウンロード中..." or "ggml-base.bin loaded"

### Step 3: Add telop source
- [ ] In a scene, click "+" → Sources → "Speech to Telop Source"
- [ ] Properties dialog opens
- [ ] Pipeline dropdown lists "SpeechToTelop-0"
- [ ] Select it; set Display Duration: 5s, Animation: Fade 0.5s
- [ ] Click OK
- [ ] Source appears in the scene (1920×200 transparent rectangle)

### Step 4: Transcription works
- [ ] Speak a Japanese sentence ending in 。 into the microphone
- [ ] Wait 3-5 seconds (chunk duration)
- [ ] Transcribed text appears on the telop overlay with fade-in animation
- [ ] Text disappears after ~5 seconds with fade-out

### Step 5: Multi-instance
- [ ] Add a second "Speech to Telop Filter" to a second audio source
- [ ] Pipeline dropdown in a new telop source lists "SpeechToTelop-0" and "SpeechToTelop-1"
- [ ] Removing a filter: its pipeline disappears from the dropdown in other sources

### Step 6: Cloud API (optional)
- [ ] In filter properties, switch Backend to "Cloud (OpenAI)"
- [ ] Enter a valid OpenAI API key
- [ ] Speak into mic → transcription appears (may be faster than whisper.cpp)
- [ ] Disconnect network → "[...]" appears after 3 retries

### Step 7: Settings persistence
- [ ] Close OBS and reopen
- [ ] Filter and source settings preserved
- [ ] Plugin loads without errors

## Expected Log Output (macOS)

```
[SpeechToTelop] Filter created: SpeechToTelop-0
[SpeechToTelop] Loading model: /Users/.../models/ggml-base.bin
[SpeechToTelop] Pipeline registered: SpeechToTelop-0
[SpeechToTelop] STT result: "こんにちは。"
```

## Known Limitations

- First launch with a new model triggers download (shown in telop overlay)
- Large models (medium/large-v3) may cause audio drop-outs on older hardware
- Windows: font rendering uses GDI+ which may look different from macOS CoreText
