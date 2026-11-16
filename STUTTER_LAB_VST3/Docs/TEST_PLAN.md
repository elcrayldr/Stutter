# STUTTER LAB — QA test plan

## Build gate
- [ ] Windows 10/11 x64 configure succeeds with Visual Studio 2022.
- [ ] Release VST3 target compiles without errors.
- [ ] VST3 bundle is discovered by FL Studio 26.
- [ ] VST3 bundle is discovered by Cubase 13.

## Capture
- [ ] Mono input captures to dual-mono without channel indexing errors.
- [ ] Stereo input preserves left/right image.
- [ ] Capture starts on the next audio block and stops without host audio interruption.
- [ ] 30-second limit stops safely.
- [ ] Repeated captures do not include stale samples.
- [ ] UI waveform remains stable during capture and stop.

## Render
- [ ] Selection bounds clamp correctly at start/end of capture.
- [ ] Repeat count 2–32 produces the expected number of slices.
- [ ] Rate divisions 1/2 through 1/64 produce expected slice lengths at host BPM.
- [ ] Reverse playback reads the selected region in reverse.
- [ ] Phone and Drive affect the rendered file only; live pass-through remains unchanged.
- [ ] Edge fades reduce clicks without audible loss of attack.
- [ ] 24-bit WAV opens in FL Studio and Cubase at the capture sample rate.
- [ ] Drag-and-drop is tested in both hosts.
- [ ] Save As export dialog writes to the selected path and opens in both hosts.

## State and reliability
- [ ] Repeat, division, reverse, Phone, Drive, and selection restore with a DAW project.
- [ ] Repeated open/close, sample-rate changes, mono/stereo layouts, and long captures do not crash.
- [ ] No allocation, file I/O, or blocking lock occurs in the audio callback.

## Release gate
No release claim until all applicable checks above are run and results recorded with host version, OS, sample rate, and buffer size.
