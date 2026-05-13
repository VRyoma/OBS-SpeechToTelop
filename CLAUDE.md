# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OBS Studio plugin that transcribes microphone input in real-time and displays stylized captions (テロップ) inspired by Japanese variety TV shows — bold, colorful, animated text overlays.

## Goals

1. Capture audio from a microphone source inside OBS
2. Run real-time speech-to-text transcription (target: Japanese + English)
3. Render the transcribed text as variety-style telop via an OBS text/graphics source

## Tech Stack (planned)

- **Language**: C++ (OBS plugin requirement)
- **Build system**: CMake
- **OBS API**: `libobs` — audio filters attach to audio sources; custom sources render video frames
- **Speech recognition**: `whisper.cpp` (local, no API key required; supports Japanese)
- **Text rendering**: OBS `obs_source_t` with custom `video_render` callback, or OBS built-in text source driven by plugin

## Build

```bash
# Prerequisites: OBS Studio source or SDK, cmake, a C++17 compiler
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
# Copy the resulting .so/.dylib/.dll into OBS's plugins directory
```

## OBS Plugin Architecture

An OBS plugin registers itself in `obs_module_load()` (entry point declared with `OBS_DECLARE_MODULE()`). Key registration points:

- `obs_register_source()` — registers a custom source (video overlay) or audio filter
- Audio filters use `struct obs_audio_info` and receive PCM frames via the `filter_audio` callback
- Video sources implement `video_render` to draw text frames each tick

Whisper.cpp integration runs on a background thread; the audio filter callback enqueues raw PCM, and the transcription thread dequeues and processes it, then pushes the result to the render thread via a lock-free queue or mutex-protected string.

## Telop Style

Variety-style telop characteristics to implement:
- Large, bold font with thick outline or drop shadow
- Per-character color variation or gradient
- Pop-in animation (scale/fade per character)
- Optional furigana or emphasis marks for Japanese text
- Configurable display duration per phrase before auto-clear
