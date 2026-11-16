# STUTTER LAB — CLAUDE HANDOFF STATUS

Repository: https://github.com/elcrayldr/Stutter
Branch packaged: the repository default branch at the workflow checkout commit.
Purpose: give Claude the complete current repository plus a concrete implementation brief.

## What this package is
- Full repository source and project files, not a rewritten sample project.
- Includes the root-level `CLAUDE_PROMPT.md` with the product specification, visual direction, DSP requirements, engineering constraints and milestone order.
- Includes GitHub Actions workflow files and existing project documentation.
- Excludes only Git metadata and transient build/IDE output from the source handoff archive.

## Baseline context known before handoff
The current project is a JUCE 8.0.6 / C++17 Windows VST3 project. Its intended main flow is capture a phrase, select a region, generate rhythmic stutters, preview the render, drag the result into the DAW and export WAV as fallback.

The earlier development work added or targeted:
- Source and generated waveforms.
- Capture and selection controls.
- Repeat/rhythmic division/reverse controls.
- Host BPM reading.
- Basic offline drive saturation and phone-style filtering.
- Rendered WAV, preview and drag-to-DAW path.
- 24-bit WAV export.

Do not assume every item above is correct or fully functional in the exact commit being packaged. Inspect the source and test it.

## Known risks to verify from source
- Whether render still runs synchronously on the UI thread.
- Whether capture/preview locks can contend with the audio callback.
- Whether the entire selected range is processed or only an initial grid-length slice.
- Whether render filenames are unique and safe while a DAW imports them.
- Whether Generate, Preview, Drag and Export are truly separate actions.
- Whether external drag/drop works in FL Studio 26 and Cubase 13.
- Whether current effects are render-only or real-time.
- Whether state recall, sample-rate changes, mono/stereo and editor close/reopen are safe.

## Hard rule
The archive is a handoff of the current code, not a claim that the requested improvements have already been implemented. Claude must audit, implement, compile and report each milestone.
