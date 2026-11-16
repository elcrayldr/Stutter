# Architecture

## Processing model
STUTTER LAB is capture-first. The host audio callback remains a transparent pass-through. Audio is copied into a preallocated 30-second stereo capture buffer. The capture lock is non-blocking on the audio thread: if the UI is taking a snapshot, that callback block is skipped rather than stalling the DAW. This is a safety compromise for the current prototype; a production implementation should replace it with a lock-free/triple-buffer snapshot strategy so no captured blocks are lost.

## UI and render
The editor displays a waveform snapshot and lets the user define a time selection. Rendering copies the captured region to a local buffer, applies the selected rhythmic slice, optional reverse, basic phone band-limiting and saturation, edge fades, and writes a 24-bit WAV. File writing and DSP rendering occur on the message/UI thread in this milestone; long renders can temporarily stall the interface. A production build should move rendering to a worker thread and report completion asynchronously.

## Current modules
- `PluginProcessor`: host adapter, capture buffer, transport BPM read, state, offline WAV render.
- `PluginEditor`: capture controls, waveform selection, render trigger, status and visual presentation.
- `CMakeLists.txt`: JUCE 8.0.6 dependency and VST3 target.
- `.github/workflows/windows-vst3.yml`: Windows x64 build and artifact publication.

## Next engineering milestones
1. Replace capture snapshot lock with a lock-free design and add capture-generation/valid-length tracking.
2. Move render to a cancellable worker thread; add Save As fallback and verified host drag/drop.
3. Implement pitch/formant, modulation, delay/reverb, per-slice envelopes, and proper parameter automation.
4. Add preset management, undoable selection editing, zoom/scroll, transient/syllable tools.
5. Validate FL Studio 26 and Cubase 13; add CI tests and release packaging.
