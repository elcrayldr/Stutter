# CRAY LDR — STUTTER LAB

Capture-based vocal stutter processor. Windows x64 VST3 target, built with JUCE 8 and CMake.

## Current implementation status (0.2.0 source milestone)

Implemented in source:
- Stereo and mono host input; live signal remains unprocessed (capture workflow).
- Up to 30 seconds of internal capture.
- Host BPM reading when supplied by the DAW.
- Waveform display and mouse range selection.
- Repeat count (2–32), rhythmic division (1/2 to 1/64), reverse playback.
- Offline render to 24-bit WAV.
- Basic Phone band-limiting and Drive saturation during offline render.
- Short edge fades on each repeated slice.
- External file drag-and-drop attempt after render.
- Save As WAV export dialog as a fallback workflow.
- Basic parameter state persistence.

Not yet implemented: pitch/formant shifting, tempo-synchronised delay, chorus, reverb/reverse reverb, preset browser, full drag/drop host validation, automated host tests, installer/signing, and a polished final GUI. The current Phone/Drive controls are render-stage processing, not real-time inserts.

## Build on Windows 10/11

Requirements:
- Visual Studio 2022 with Desktop development with C++
- CMake 3.22+
- Git and an internet connection for JUCE FetchContent on first configure

Run from the repository root:

```powershell
cmake -S STUTTER_LAB_VST3 -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target STUTTER_LAB_VST3 --parallel
```

Expected artifact:

`build\STUTTER_LAB_artefacts\Release\VST3\STUTTER LAB.vst3`

Copy the `.vst3` bundle to:

`C:\Program Files\Common Files\VST3\`

Rescan plugins in FL Studio 26 or Cubase 13.

## GitHub Actions

The Windows workflow is located at `.github/workflows/windows-vst3.yml`. Run **Build STUTTER LAB VST3 (Windows)** from Actions, or push a change to `STUTTER_LAB_VST3/**` on `main`/ `master`. The workflow builds the x64 VST3 and publishes it as an Actions artifact.

## Important

This is an active development source milestone, not a release-certified plugin. No successful compile or DAW validation is claimed until the Actions build and manual host tests have passed.
