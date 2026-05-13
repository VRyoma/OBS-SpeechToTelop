# SpeechToTelop — OBS Plugin Design Spec

## Overview

OBS Studio plugin that transcribes microphone input in real-time and displays stylized captions (テロップ) inspired by Japanese variety TV shows. Designed for live streaming use.

## Requirements

- **Language**: Japanese (extensible to English later)
- **STT**: whisper.cpp (local) + cloud API (OpenAI Whisper API) selectable
- **Display**: Single-line telop with configurable variety-show style effects
- **Platform**: macOS + Windows
- **Latency/accuracy**: User-adjustable balance

## Architecture

Three-thread pipeline connected by lock-free structures:

```
[OBS Audio Filter]                    [OBS Video Source]
  | PCM enqueue                         ^ read text
[Audio Ring Buffer]  ->  [STT Thread]  ->  [Text Queue]
                            |
                     [whisper.cpp / Cloud API]
```

### Components

1. **Audio Filter** (`audio-filter`) — OBS audio filter registered via `obs_register_source`. Attaches to a mic source, enqueues PCM frames into a lock-free ring buffer.

2. **STT Engine** (`stt-engine`) — Background thread. Dequeues PCM from ring buffer, runs inference via whisper.cpp or cloud API, pushes results to a mutex-protected text queue.

3. **Telop Source** (`telop-source`) — OBS custom video source. Reads latest text from the queue, renders telop via `video_render` callback using libobs graphics API.

### Shared State

A singleton `SpeechToTelop` instance bridges all components. Both the audio filter and telop source access it via OBS `void*` context.

- Audio Filter -> STT Engine: Lock-free ring buffer (high-frequency, continuous audio stream)
- STT Engine -> Telop Source: `std::mutex` + `std::string` (low-frequency text updates)

## STT Engine

### Backends

**whisper.cpp (local):**
- Audio chunked into configurable length (1-10 seconds) for inference
- Context tokens from previous chunk passed to next chunk for continuity
- Models: tiny (~75MB), base (~142MB), small (~466MB), medium (~1.5GB), large (~2.9GB)
- Model selection via preset dropdown + custom path option
- Model loaded into memory on first use, reused for subsequent inference

**Cloud API (OpenAI Whisper API):**
- API key configured in OBS properties (marked as sensitive data)
- Synchronous HTTP requests in STT thread with timeout
- Retry up to 3 times on failure

### Model Distribution

- Preset models auto-downloaded on first use to OBS module data directory
- Download progress shown on telop: "モデルをダウンロード中..."
- Custom models: user provides path to `.bin` file

### Text Display Modes

User-selectable in settings:

- **Sentence-boundary mode** (default): Buffer text until sentence-ending punctuation (。！？.!?) or timeout. Prevents mid-word breaks.
- **Instant mode**: Display inference results immediately, finalize at sentence boundaries.

Sentence-boundary mode timeout is configurable (default: 5 seconds).

### Error Handling

| Condition | Response |
|-----------|----------|
| Model file missing | Auto-download begins, telop shows "モデルダウンロード中..." |
| Download failed | Telop shows "ダウンロード失敗" + retry prompt |
| Cloud API error | Retry up to 3 times, show `[...]` on failure |
| No audio (silence) | Skip inference to save CPU |
| Memory pressure | Unload/reload model |

## Telop Renderer

### Basic Rendering

- Rendered via libobs `gs` graphics API in `video_render` callback
- Position: bottom-center by default, adjustable via source properties
- Single-line display: new text replaces old text

### Effects (all configurable ON/OFF)

| Effect | Implementation | Settings |
|--------|---------------|----------|
| Bold + outline + shadow | Multi-pass text rendering (shadow -> outline -> body) | Outline thickness, shadow offset, colors |
| Per-character color | Change `gs` color per character | Color palette, cycle mode |
| Per-character pop-in | Staggered scale-in per character | Animation speed |
| Enter/exit animation | Fade in/out, slide in | Animation type, duration |

### Animation Control

- `video_tick` callback tracks elapsed time
- New text received -> enter animation begins
- Configured display time elapsed OR next text received -> exit animation -> display ends
- Per-character animations use per-character timing offsets

### Text Management

- Struct holds "current text" and "animation state"
- Streaming inference updates handled smoothly: extension of same sentence updates in-place

## OBS Plugin Integration

### Entry Point

- `OBS_DECLARE_MODULE()` + `obs_module_load()`
- Registers two sources:
  - Audio filter: `obs_register_source(&audio_filter_info)`
  - Video source: `obs_register_source(&telop_source_info)`

### User Setup Flow

1. Add "Speech to Telop Filter" to an audio source (mic) in OBS
2. Add "Speech to Telop Source" to a scene in OBS
3. Configure properties on the telop source

### Settings UI

**STT Settings:**

| Item | Type | Default |
|------|------|---------|
| STT Backend | Dropdown | Local (whisper.cpp) |
| Model Selection | Dropdown | base |
| Custom Model Path | Text | (empty) |
| Speed/Accuracy Balance | Slider | Center (3s chunks) |
| Language | Dropdown | Japanese |
| Text Display Mode | Dropdown | Sentence-boundary |
| Buffer Timeout | Slider | 5s |
| OpenAI API Key | Text (password) | (empty) |

Speed/Accuracy Balance slider label: "反応速度" (left) to "精度重視" (right), mapping to 1s-10s chunk length internally.

**Telop Settings:**

| Item | Type | Default |
|------|------|---------|
| Font | Font picker | Noto Sans CJK JP Bold |
| Font size | Slider | 48 |
| Position | Dropdown | Bottom center |
| Outline | Checkbox | ON |
| Outline thickness | Slider | 3px |
| Outline color | Color picker | Black |
| Drop shadow | Checkbox | ON |
| Per-character color | Checkbox | OFF |
| Color palette | Multi color picker | White, Yellow, Red, Blue |
| Pop-in animation | Checkbox | OFF |
| Pop-in speed | Slider | Medium |
| Enter/exit animation | Dropdown | Fade |
| Animation duration | Slider | 0.5s |
| Display duration | Slider | 5s |

### Instance Coordination

Audio filter and telop source share the `SpeechToTelop` singleton. If multiple filters are attached, only the first one is active.

## Build & Cross-Platform

### File Structure

```
SpeechToTelop/
├── CMakeLists.txt
├── src/
│   ├── plugin-main.cpp
│   ├── speech-to-telop.h/cpp
│   ├── audio-filter.h/cpp
│   ├── stt-engine.h/cpp
│   ├── stt-whisper.h/cpp
│   ├── stt-cloud.h/cpp
│   ├── telop-source.h/cpp
│   ├── telop-renderer.h/cpp
│   ├── telop-animation.h/cpp
│   └── model-downloader.h/cpp
├── vendor/
│   └── whisper.cpp/
└── data/
    └── locale/
```

### Dependencies

- **libobs** — OBS SDK (path specified at build time)
- **whisper.cpp** — git submodule or FetchContent
- **libcurl** — Model download + cloud API (system library)
- **C++17**

### Platform

- macOS: `.dylib`, standard OBS plugin directory
- Windows: `.dll`, MSVC or MinGW
- Platform differences handled via `#ifdef`
- CI: GitHub Actions for both platforms

## Out of Scope (Future Extensions)

- English language support
- Multi-line stacked display
- Furigana rendering
- Telop design presets ("Variety", "News", etc.)
