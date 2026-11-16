# CLAUDE CODE — MASTER BRIEF
## CRAY LDR / STUTTER LAB — JUCE VST3

You are taking over an existing C++ audio plug-in project. The complete repository is included in this package. Work on the existing codebase; do not replace it with a mock-up, a web app, a standalone-only prototype, or a new empty JUCE template.

## 1. Product goal

Build STUTTER LAB into a premium, reliable vocal phrase/stutter design VST3 for Windows 10/11 x64, primarily used in FL Studio 26 and Cubase 13.

The essential user workflow is:

**CAPTURE → SELECT → GENERATE → PREVIEW → DRAG TO DAW**

The plug-in must capture audio from its input, let the user select a phrase or syllable, create rhythmic edits and effects, audition the result inside the plug-in, regenerate variations without losing the original capture, and drag the exact generated audio into the DAW. A 24-bit WAV export must always remain available as a fallback.

Target toolchain: JUCE 8.0.6, C++17, CMake, Visual Studio 2022, Windows x64, VST3.

The user works in FL Studio 26 and Cubase 13. Do not assume drag-and-drop or host behaviour works until it has been tested in those DAWs.

## 2. First: inspect the actual repository

Before editing:
1. Inspect every source file, CMake file, workflow, resource, and existing document.
2. Identify the current processor/editor architecture, parameter/state model, DSP path, capture/render flow, and current build target.
3. Read the existing git history where useful.
4. Make a concise risk list. Do not assume any feature is working merely because a button or method exists.
5. Preserve current working functionality and plug-in identity.

The source in this archive is authoritative. Do not paste in a different implementation from a previous draft or invent missing files.

## 3. Visual direction — Abbey Road / Waves-inspired, original execution

The user wants the character of the Waves Abbey Road collection: classic British recording/mastering hardware, vintage studio panels, tactile controls and premium analogue equipment. Use that as a mood/reference only. Do NOT copy Waves or Abbey Road plugin interfaces, logos, product names, exact layouts, proprietary graphics, artwork, typography or trade dress.

Create a distinct CRAY LDR identity:
- Premium British studio hardware aesthetic; restrained, engineered, not gaming/EDM neon.
- Deep charcoal, aged graphite, warm black, muted olive/bronze, brushed champagne brass and warm ivory.
- Optional subtle wood/metal side cheeks only if they improve the composition.
- Engraved or screen-printed section labels; high-quality tactile rotary knobs with calibrated ticks and readable numeric values.
- Large analogue-inspired VU / peak meters with believable ballistics, needle damping and peak hold where relevant.
- Waveform displays should look like recessed hardware displays, but remain highly readable and functional.
- Fine panel seams, screws, subtle shadows, restrained highlights and realistic material hierarchy. Avoid excessive bevels and fake skeuomorphism.
- Clear functional groups: INPUT/CAPTURE, EDIT, PATTERN, TONE, PITCH, SPACE, OUTPUT.
- Header: CRAY LDR / STUTTER LAB.
- Primary actions must be visually obvious: CAPTURE, GENERATE, PREVIEW/STOP, DRAG TO DAW, EXPORT WAV.
- No tiny text, no controls that are only decorative, no dead buttons.
- Build a reusable custom JUCE LookAndFeel and reusable UI components rather than drawing the entire interface inside one paint() method.
- Support resize, high-DPI Windows scaling and minimum 860x570 layout. Test 100%, 125%, 150% and 200% display scaling.
- Keep the interface coherent at the minimum size and at large windows.

Do not sacrifice waveform editing usability for visual styling. Function first, visual finish second.

## 4. Core workflow — required

### Capture
- Capture mono or stereo input without blocking or allocating in processBlock().
- Record up to a sensible configurable duration (initial target 30–60 seconds).
- Clearly show capture state, elapsed time, input peak and clipping.
- Start/stop must be deterministic and safe during host transport changes.
- Re-capturing must not leak stale samples into the next take.
- Preserve the captured audio while the user generates multiple versions.

### Select and edit
- Source waveform with zoom, horizontal scroll, selection handles and clear selected-range display.
- Select a word, syllable, transient or any arbitrary region.
- Snap-to-grid and free selection modes.
- Selection start/end and duration readouts.
- Loop audition of the selection.
- Undo/redo for selection changes.
- Transient detection and suggested slice markers may be added, but user must retain manual control.
- Never silently shorten a selected region or only process its first few milliseconds. Full selection must be available to the slicing/pattern engine.

### Generate
- Separate Generate, Preview, Stop, Drag and Export actions.
- Generation must not automatically open a system drag operation.
- Render runs on a background worker, with progress and cancellation.
- Keep source capture intact when changing settings or generating again.
- Each render gets a unique, persistent temporary filename; never overwrite a file the DAW may still be importing.
- Generated waveform and metadata update only after a render completes successfully.
- Support deterministic random seeds so a saved preset/project can reproduce the same pattern.

### Preview
- Preview must play the exact rendered buffer from the plug-in output.
- Start/stop and playhead must be sample-accurate enough for reliable audition.
- Normal pass-through must remain unchanged when preview is off.
- Do not introduce audible glitches when switching between pass-through and preview.
- No locks, memory allocation, file I/O or buffer resizing in the audio callback.

### Drag and export
- Drag the exact render WAV into FL Studio 26 and Cubase 13.
- If a host does not accept the drag, retain the Save As WAV fallback.
- WAV export: source sample rate by default, stereo/mono handled correctly, 24-bit PCM minimum.
- Optional 32-bit float export can be added.
- Report render duration, sample rate, bit depth and peak.
- Do not claim host drag/drop success without manual host testing.

## 5. DSP and effect rack — real functions, not decorative controls

Implement effects as an offline render chain first if needed for reliability. Once the offline DSP is correct, add optional real-time preview/monitoring. Every control must audibly work and be automated/persisted where appropriate.

### A. Stutter and pattern engine
- Rhythmic divisions: 1/2, 1/4, 1/8, dotted 1/8, 1/8T, 1/16, dotted 1/16, 1/16T, 1/32, 1/32T, 1/64.
- BPM from host transport, with manual BPM fallback when host tempo is unavailable.
- 16-step minimum pattern sequencer.
- Per-step on/off, slice position, repeat count, gate length, level/velocity and probability.
- Swing 0–100%.
- Humanize/time variation with safe limits.
- Forward, reverse, ping-pong, alternating and random-with-seed modes.
- Repeat count, step rate, pattern length and loop mode.
- Short micro-fades or equal-power crossfades to avoid clicks without dulling consonants/transients.
- Avoid low-frequency/DC accumulation from repeated slices.

### B. Gate / volume pattern
- Tempo-synced trance/volume-gate style patterns.
- Gate length, attack, release, depth and step pattern.
- Smooth gain transitions; no zipper noise or clicks.
- Optional sidechain-style rhythmic envelope only if the source/host architecture supports it cleanly.

### C. Tone
- Drive/saturation: soft clip, tape-like and harder transistor/console-style modes if implemented convincingly.
- Input drive, output trim and wet/dry.
- High-pass and low-pass with frequency and slope controls.
- Phone/radio/megaphone tone presets using proper filtering; do not make Phone a single crude hard-coded filter.
- Bit depth reduction and sample-rate reduction with mix control.
- Optional resonant filter only if stable and well-behaved.
- Gain-match bypass for meaningful A/B comparisons.

### D. Delay and space
- Tempo-synced delay: 1/4, dotted 1/8, 1/8, 1/8T, 1/16, 1/16T, 1/32.
- Feedback bounded to a safe range (maximum 90%).
- Wet/dry 0–100%.
- Stereo/ping-pong mode and L/R offset.
- Filtered feedback path.
- Reverb with size, damping, pre-delay and wet/dry.
- Optional reverse tail/reverse reverb rendered offline if stable.
- Prevent feedback runaway and excessive CPU.

### E. Pitch and formant
- Pitch shift at least ±12 semitones and fine tune in cents.
- Formant control separate from pitch only if a real formant-preserving algorithm is available.
- Do not fake formant shifting with EQ.
- Evaluate audio quality, latency, transient smearing and artefacts on vocal material.
- If a dependency is needed, explain its licence, redistribution terms and build impact before integrating it.
- Do not claim transparent studio-quality pitch shifting unless listening tests support that claim.

### F. Output
- Output trim, peak meter, peak hold and clipping warning.
- Optional export limiter with OFF as default.
- No automatic loudness normalization by default; preserve dynamics.
- Keep true-peak overs and inter-sample risk in mind for export.
- Metering must be real signal metering, not animation unrelated to audio.

## 6. Presets and production workflow
- Factory presets aimed at modern reggaeton, trap, drill, pop/urban vocal FX, ad-libs and clean utility stutters.
- Preset browser with categories and search.
- Save/load user presets.
- A/B compare.
- Undo/redo for parameter and selection snapshots.
- Randomize with per-section locks and deterministic seed.
- Parameter tooltips, sensible ranges and safe defaults.
- All relevant parameters exposed through AudioProcessorValueTreeState for host automation.
- Stable parameter IDs; versioned state format and migration from the current project state.
- Session recall must restore parameters and user-visible state safely.

## 7. Real-time and code quality rules

These are non-negotiable:
- No blocking locks, allocations, file I/O, UI calls, buffer resizing or unbounded work inside processBlock().
- Use preallocated audio buffers and safe ownership/lifetime rules.
- Replace shared spin-lock designs with a proven lock-free or carefully designed double/triple-buffer publication approach.
- Do not access editor objects from the audio thread.
- Worker-thread callbacks must not call a destroyed editor.
- Avoid data races between render worker, audio callback, UI and waveform generation.
- Handle mono/stereo, sample-rate changes, variable block sizes, host stop/start, transport loops and editor close/reopen.
- Use JUCE DSP modules where appropriate, but keep DSP separate from UI.
- Use clear class boundaries: audio processor/engine, capture store, render job, pattern model, effect chain, waveform/editor components, custom LookAndFeel and preset manager.
- Keep JUCE pinned to 8.0.6 unless a justified compatibility issue requires a change.
- Keep C++17 and the existing VST3 identity/bundle ID unless there is a technical reason to change.
- Do not introduce unverified APIs. Inspect JUCE headers and compile.
- No fake meters, placeholder controls, empty callbacks or TODO-only features in a claimed-complete milestone.
- Add tests for timing, slice boundaries, fades, effect stability and state migration.
- Keep GitHub Actions build green after each coherent change.

## 8. Audio quality acceptance criteria

Test with spoken and sung vocals, including sharp consonants, breathy takes, aggressive reggaeton/trap delivery and sustained vowels.
- No clicks at slice boundaries.
- No unintended clipping or DC offset.
- Consonants and transient attacks remain intelligible.
- Saturation bypass is level-matched.
- Delay feedback remains stable.
- Gate timing remains locked to host BPM.
- No audible output change in normal pass-through mode with all effects inactive.
- Test 44.1, 48, 88.2 and 96 kHz.
- Test block sizes 64, 128, 256, 512 and 1024 where the host supports them.
- Compare output renders at matched loudness; do not confuse louder with better.

## 9. Milestone order — do not build everything in one unreviewable rewrite

### Milestone 0 — audit and baseline
- Inspect the full repo.
- Record the exact current build status.
- Identify actual working features and defects.
- Do not change code until the audit is complete.

### Milestone A — correctness and safe engine
1. Fix full-region selection and slicing.
2. Correct rhythmic timing against host BPM and triplet/dotted divisions.
3. Separate Generate / Preview / Drag / Export.
4. Unique render filenames.
5. Background render worker, cancellation and lifetime-safe completion.
6. Safe capture/preview publication without RT locks.
7. Test and build.

### Milestone B — interface
1. Create custom LookAndFeel and reusable components.
2. Redesign with original Abbey Road / Waves-era hardware-inspired visual language.
3. Add proper dual waveform editor and selection handles.
4. Add genuine meters and readable rotary controls.
5. Resize/DPI tests.
6. Build and inspect screenshots.

### Milestone C — effects
Implement and test in this order:
1. Pattern sequencer, swing, gate and reverse modes.
2. Filter, saturation and bit/sample-rate reduction.
3. Tempo delay and stereo/ping-pong.
4. Reverb and reverse tails.
5. Pitch/formant only after selecting a credible algorithm and assessing quality.
Build and test after each effect group.

### Milestone D — production workflow
1. Presets, A/B, undo and randomization.
2. State migration and automation.
3. FL Studio 26 validation.
4. Cubase 13 validation.
5. Package VST3 and source.
6. Document installation and known limitations.

## 10. Required reporting after every milestone

Report:
- Files changed.
- Actual behaviour implemented.
- DSP details and parameter ranges.
- Build result and GitHub Actions run link.
- Tests performed and exact outcomes.
- What is not tested yet.
- Any known defects and next step.

Never say “done”, “professional quality”, “production-ready”, “drag-and-drop works”, “pitch/formant is transparent”, or “zero latency” unless that specific claim has been demonstrated.

## 11. Start now

First audit the entire repository and provide the architecture/risk report. Then implement Milestone A in small, buildable commits. Do not start with a mock-up or visual-only work. Preserve the current code and working features. Build and test before proceeding to the visual overhaul.
