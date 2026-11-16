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
- Windows GitHub Actions build workflow.

Not yet implemented: pitch/formant shifting, tempo-synchronised delay, chorus, reverb/reverse reverb, preset browser, full drag/drop host validation, automated host tests, installer/signing, and a polished final GUI. The current Phone/Drive controls are render-stage processing, not real-time inserts.

## Build on Windows 10/11

Requirements:
- Visual Studio 2022 with Desktop development with C++
- CMake 3.22+
- Git and an internet connection for JUCE FetchContent on first configure

```powershell
cmake -S . -B build -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target STUTTER_LAB_VST3 --parallel
```

Expected artifact:

`build\\STUTTER_LAB_artefacts\\Release\\VST3\\STUTTER LAB.vst3`

Copy the `.vst3` bundle to:

`C:\\Program Files\\Common Files\\VST3\\`

Rescan plugins in FL Studio 26 or Cubase 13.

## GitHub Actions

Push this repository to GitHub and run **Build STUTTER LAB VST3 (Windows)** from Actions, or push a change to `main`/`master`. The workflow builds the x64 VST3 and publishes it as an Actions artifact. This workflow has been authored but has not yet been executed in this environment.

## Important

This is an active development source milestone, not a release-certified plugin. The Linux session used to edit this source has no network access to fetch JUCE, so no successful compile or DAW validation is claimed. Test the resulting artifact in both target DAWs before distribution.
