# Skell-Glitch — CUT IT. FLIP IT.

## v1.5.3 — idle-friendly 32-step UI rendering
- Cached direct atomic pointers to the 32 step-state/probability parameters, removing JUCE parameter-ID construction and lookup from each grid paint.
- Grid now redraws only when the playhead step, visible step count, or a gate/probability parameter actually changes; no more unconditional 30 Hz repaints while stationary.
- Mouse-painted gate and probability edits now repaint immediately, while external automation updates continue to appear through lightweight change detection.


## v1.5.2 — pitch-consistent ping-pong and peak-safe reverse playback
- Fixed Ping-Pong mode's forward half ignoring reverse-pitch controls. Both playback directions now use the same overlap-grain pitch processing with correctly oriented read offsets.
- Corrected Tape mode to slow down toward the end of each slice instead of accelerating.
- Four-point capture interpolation now stays within the neighbouring sample range, avoiding interpolation-created peak overshoots on sharp near-full-scale transients without altering dry audio.
- Added DSP regressions for pitched forward ping-pong, tape deceleration, transparent unpitched forward playback and bounded interpolation of square transients.


## v1.5.1 — responsive selection and gate automation painting
- Selected loop length and reverse/gate/both mode now render correctly at startup, without waiting for a 20 Hz timer. Button clicks update active appearance and combo visibility immediately; unchanged selections no longer trigger periodic repaints.
- Gate-step probability can be painted intuitively across the full step height while holding Shift, or via the narrow probability strip. A single host gesture stays open during continuous dragging and closes when switching steps or releasing the mouse, avoiding repeated automation begin/end events on every pixel.
- Offscreen JUCE harness asserts that the initially selected loop/mode buttons are highlighted and that action callbacks update both host parameters and button states immediately.


## v1.5.0 — probability-correct reverse steps and robust sidechain triggers
- Resolved sequencer probability once per step for both gain and reverse state. Previously reverse-enabled gate steps stopped reversing whenever the gate's closed gain equalled the open gain; this no longer happens.
- A 0% probability reverse step never reverses, 100% always reverses, and fixed-seed decisions remain deterministic across repeated render passes.
- Sidechain triggering now treats non-finite samples as silence and sanitises trigger velocity/note, preventing indefinite detector corruption by upstream NaNs/Infs.
- Regression tests cover all the above without introducing processing allocations or additional dependencies.


## v1.4.3 — screenshot-driven UI fixes
- Inspected the actual offscreen renders at 479x338, 719x507, 958x676, and 1198x845. When input is silent, waveform output no longer draws a false flatline or bright playhead over the waiting message.
- Moved the version label away from the bottom-right JUCE resize grip so it cannot be truncated at any window size.


## v1.4.2 — full processor and automatable UI smoke
- Added a real JUCE AudioProcessor block test with a known anti-phase sine input: exact bypass pass-through, zero-latency mode, finite and below-unity outputs, active reverse processing, and phase-coherent stereo.
- Offscreen UI smoke also invokes the actual gate/loop button callbacks and verifies that their automatable parameters change correctly.
- MIDI note-ons now read fixed-size event bytes directly, avoiding MidiMessage object creation and potential variable-size copies in the audio callback.
- CI prints detailed stress throughput diagnostics with verbose CTest output on Linux and Windows. The tests still do not replace pluginval/FL Studio host profiling or real audio listening.


## v1.4.1 — UI render smoke test and extreme DSP regression
- Added a JUCE offscreen screenshot harness to Windows CI: 50%, 75%, 100% and 125% canvas sizes are rendered and checked for a valid nonblank image; PNG snapshots are uploaded as an inspectable artifact.
- Added deterministic reverse/grain/gate/mix stress checks for finite outputs, below-unity amplitude and stereo anti-phase consistency, plus a timing diagnostic; the synthetic test does not replace audio listening or host CPU profiling.
- Gate exponential tails now zero inaudible residuals before they enter the denormal range.


## v1.4.0 — verified transport, gain, warning and idle-render fixes
- Sanitize invalid BPM, PPQ, free-length and sample-rate values before computing clock divisions or integer slice positions; cache clock divisions once per host update rather than per sample.
- Use constant-gain dry/wet interpolation so a correlated dry/wet 50% mix does not add ~3 dB. This is not a peak limiter: intentionally accented or hot wet audio can still exceed 0 dBFS.
- Replace deprecated JUCE MIDI iterator with the JUCE 8 range iterator, retaining sample-accurate note-on offsets.
- Migrate font creation to JUCE 8 FontOptions; keep visual geometry unchanged.
- Ignore non-finite input samples before IIR filtering so corrupted upstream buffers cannot poison the filter state.
- Skip idle waveform repaints when transport and relevant displayed state are unchanged.
- Add regression assertions for invalid host transport values, non-finite controls, unity-preserving mix and preexisting block-size consistency.


## v1.3.0 — responsive controls and leaner real-time DSP
- The compact 75%-scale default and all existing skins/controls remain intact. Factory/preset toolbar uses readable short labels (A+, A USE, B+, B USE, SAVE, OPEN), with functional hover tooltips and safe asynchronous file picker callbacks.
- Gate steps now show hover outlines and beat-quarter indices; waveform/grid painting pauses when those components are hidden. Tooltip window is enabled in the editor.
- Reverse-pitch prepares the pitch ratio once per audio block and uses a 2,048-point linearly interpolated Hann table rather than expensive cosine calculations per sample. Reverse styles and pitch range remain unchanged.
- Equal-power mix coefficients are calculated once per sample for stereo rather than separately per channel; 0% and 100% mix return exact endpoint gains.
- Linear gate envelope reaches attack and release targets at a defined sample count; other gate curves retain their characteristic smoothing. Reverse states in GATE mode now respect the gate shaper.
- Mono processing allocates only one channel of aligned delay history. Hermite capture reads now use one wrapped ring index plus local offsets rather than multiple 64-bit modulo operations.
- Expanded dependency-free regressions cover precise linear attack/release, reverse-pitch context consistency, equal-power endpoints, and Hermite reads crossing a circular-buffer wrap.

**Verification:** passing DSP unit tests and a successful CI compilation establish baseline correctness, not subjective sound quality. FL Studio host playback, transient listening, CPU profiling, and 50% UI click-testing remain to be done.


## v1.2.0 — compact interface and smooth splice engine
- Default editor 719×507 (75% design scale, half the 150% screenshot dimensions), with XS 50% (479×338), M 100%, and L 125%. Legacy session sizes open at the compact default.
- Rebalanced waveform/gate/mode/preset/band rows; fixed raw floating point knob text and broken icon glyphs with vector power/settings icons plus ASCII shift labels.
- Reduced redundant header/loop redraws; waveform/grid now refresh at 30 Hz; silent audio display shows WAITING FOR AUDIO.
- SMOOTH now bridges reverse slice boundaries using the outgoing wet sample rather than fading to zero; FADE IN/OUT remain independently adjustable. Added regression tests.
- Reduced capture/delay history from 64 to 32 seconds, halving worst-case RAM needs; mono hosts no longer reserve a right capture channel. Extremely slow long slices remain unsupported beyond 32 seconds.
- Host QA still needed: clickless transient bounces, FL Studio rescan, high-sample-rate CPU, and minimum-window usability.


## v1.1.0 refinement
- Cached per-block gate envelope coefficients and per-step probability decisions to reduce audio-thread work.
- Reset capture indices without clearing large preallocated audio buffers during transport jumps.
- Clear stale waveform peaks when no completed slice is available; honor probability-disabled reverse steps.
- Stereo sidechain envelope triggering; JUCE main bus DSP remains lock-free in steady-state.
- Improved neon band rail/handles, hover feedback, Shift fine frequency adjustment, and idle repaint suppression.
- Cyan probability meters on gate steps and Shift + drag probability painting.
- Added cached-envelope equivalence and transport-reset regression tests.

This revision is validated by dependency-free tests locally; native host/plugin validation is tracked by GitHub Actions.

A new, original **JUCE 8 C++** audio-effect source project for tempo-locked reverse slices, step gating, or both. Builds as **VST3** and **Standalone** on Windows/macOS when JUCE and a matching native compiler are available.

> **Verification:** The dependency-free DSP regression tests compile and pass on Linux (GCC 14). The complete JUCE plugin and user interface have **not** been compiled, executed or pluginval-tested in this delivery, because JUCE and network download are not available in the execution environment. Treat this as a substantial source implementation that still requires real host/GUI verification, not a release-ready binary.

## Layout

The interface uses the user-provided HalfSpeed-style *layout proportions* as a reference only, with original **Skell-Glitch** branding, a vector-drawn clock symbol, bone-white lettering, necro-green (#39ff3a), magenta for reverse and cyan for gate states. Its design canvas is **958×676**. Preset window sizes are **XS 50%, S 75% (default), M 100%, L 125%**, with a fixed aspect ratio and a scaled JUCE canvas. The top bar shows sample rate and reported latency.

Controls: sync, nine loop lengths, fade-in/out rings, waveform/playhead, 1–32 gate steps with per-step probability, pattern operations and pattern presets, Reverse/Gate/Both selectors, reverse style, gate rate, smooth, two-handle band limits, mix, power, factory presets, undo/redo, A/B snapshots and XML export/import. The **settings** drawer has trigger source, latency mode, chaos/seed, pitch, gate envelope controls, MIDI/manual triggering, freeze, delta listen, band inversion, gate order and window sizes.

The project embeds one **original SVG mark** using `juce_add_binary_data`. It **does not redistribute font files**. The UI uses system Arial/Consolas with normal OS fallback rather than embedded Rajdhani/JetBrains Mono fonts.

## Audio architecture

The audio path uses only preallocated buffers during steady-state `processBlock` and stores automatable controls in `AudioProcessorValueTreeState`. DSP modules under `dsp/` are header-only and JUCE-independent so they can be unit-tested quickly.

1. **TempoClock:** calculates slice boundaries from host PPQ, tempo, time signature and division, with 120 BPM/4-4 fallback and free-running millisecond mode. Re-aligns on transport jumps, repeats and tempo changes.
2. **BandSplitter:** uses cascaded two-stage Butterworth filters (LR4-like magnitude slopes) for the selected band. Its out-of-band signal is derived as `outside = dry - inside`, allowing exact reconstruction when unprocessed. This is a transparent *complementary split*, not a verified phase-linear crossover.
3. **SliceCapture:** writes the selected band to a preallocated circular buffer; on each slice boundary, the last completed slice becomes available for reverse playback. Hermite interpolation reads the slice; a 160-bin atomic waveform peak cache feeds the UI.
4. **Reverser:** previous-slice Classic reverse, Tape-shaped read curves, alternating Ping-Pong, and reverse Stutter. A dual overlap Hann-grain read variation provides a basic ±12 semitone pitch effect; this is not a production-grade formant-preserving pitch shifter.
5. **GateSequencer/GateShaper:** sample-rate step timing, 1–32 programmed states, accent gain, deterministic probability, swing, envelope attack/release and selected curve. The 16-step display can expand to 32 narrower steps.
6. **BOTH:** default processing order is **reverse the captured slice, THEN apply the gate**. The settings switch **GATE → REVERSE** captures gated content first and then reverses it. The gate pattern is evaluated in the reversed playback timeline for the default order.
7. **GlitchRandomizer:** chaos events (forward flip, skip, half-time, stutter) are chosen using an integer hash of seed and musical slice/bar. With SEED LOCK, the same timeline yields identical choices between renders; without it, hashed bar position produces varied but offline-repeatable decisions.
8. **TriggerRouter:** continuous auto mode, MIDI note-on bursts (notes C2–G#2 mapped to the nine durations; velocity influences gate level), sidechain-energy-onset bursts (a fast envelope-difference proxy), and a host-automatable manual trigger. Manual hold-freeze suspends capture and repeats the retained slice.
9. **Mix/latency:** equal-power dry/wet mix, a short click-softened bypass ramp and dry-aligned delta listen. **ZERO** reports 0 samples latency and reverses the *previous* captured slice; **ALIGNED** reports a slice-length delay and delays the dry/out-of-band signal to align to reverse playback. Host latency update is dispatched asynchronously to the message thread.

### Known behavioral limits / unfinished items

- **Host/UI builds:** source exists but full VST3/Standalone compile, FL Studio, Ableton, Logic and pluginval acceptance have not yet been verified.
- **FFT spectral flux sidechain:** current trigger is an envelope-difference onset proxy, not true multi-bin spectral-flux analysis.
- **MIDI learn and numeric popups:** right-click MIDI learn and direct numeric entry on the custom band slider are not built yet. Gate step painting changes parameters with complete per-step gestures; not a single batched gesture.
- **DSP refinements:** Tape mode uses a shaped reverse read index instead of a calibrated physically modeled tape transport; granular pitch does not preserve formants; gate curves approximate linear/exponential/S-curve; the shaper currently uses short fade-to-silence envelopes at slice edges, not full adjacent-slice overlap. The band split reconstructs exactly without processing, but perfect *phase alignment of individually processed bands* has not been proven.
- **Huge tempo divisions:** buffer allocation supports at most ~32 s of a completed slice. If 8 bars is longer than 32 s at exceptionally slow tempos, reverse capture will fall back while gating remains active. Buffer memory can be significant at high sample rates.
- **Window behavior and VBlank:** background panels are image-buffered, wave and grid repaint using a 60 Hz timer (not a native VBlankAttachment). Custom resize transforms need manual HiDPI and mouse-interaction validation. Title jitters ±2 px when a slice flips if enabled.
- **A/B and presets:** full project state and A/B XML snapshots serialize via JUCE state. Extra UI state (size, pattern name, order) is saved in a ValueTree; snapshot transitions are not themselves crossfaded.
- **No hardware/host bounce validation:** the independent C++ tests are not a substitute for audio click-testing, low-level latency verification, or real VST3 regression.

## Build on Windows (MSVC)

Requires **CMake 3.22+**, **Visual Studio 2022 or 2026 C++ tools** and a local JUCE 8 checkout. Use a local JUCE path to avoid fetching dependencies during configuration:

```powershell
git clone --branch 8.0.10 --depth 1 https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build -DJUCE_DIR="$PWD/JUCE"
cmake --build build --config Release --target SkellGlitch_VST3 SkellGlitch_Standalone --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Expected VST3 bundle: `build/SkellGlitch_artefacts/Release/VST3/Skell-Glitch.vst3`; Standalone: `build/SkellGlitch_artefacts/Release/Standalone/Skell-Glitch.exe` (path or extensions can differ by JUCE/toolchain). To manually install a Windows VST3, copy the `.vst3` bundle under `C:\Program Files\Common Files\VST3\` and rescan your DAW after successful compilation.

## Build on macOS (Xcode/Clang)

```bash
git clone --branch 8.0.10 --depth 1 https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build -G Xcode -DJUCE_DIR="$PWD/JUCE"
cmake --build build --config Release --target SkellGlitch_VST3 SkellGlitch_Standalone --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Expected VST3 bundle: `build/SkellGlitch_artefacts/Release/VST3/Skell-Glitch.vst3` and Standalone `.app` under `build/SkellGlitch_artefacts/Release/Standalone/`. For DAW use, install under `~/Library/Audio/Plug-Ins/VST3` or `/Library/Audio/Plug-Ins/VST3` after a successful build.

If `JUCE_DIR` is omitted and no JUCE CMake package is installed, the project uses CMake `FetchContent` to download pinned JUCE 8.0.10 during configuration; that requires an internet connection.

## Lightweight DSP tests without JUCE

```bash
cmake -S . -B build-tests -DSKELL_DSP_ONLY=ON
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

The regression test includes host-grid boundary checks, 1-, 64-, 512-, 4096- and 16384-sample block comparisons using sine/kick test inputs (within `2e-6` float sample amplitude), finite sample-rate tests, 2-channel capture/interpolation, gating/swing/probability, deterministic chaos and default/band-split null checks. Future acceptance criteria: automated pluginval, real DAW scan, transport jumps, actual offline bounce hashes for fixed block size, clicks at boundaries, mono/stereo sidechain tests, audio latency measurements, CPU profiling and all GUI clicks.

## Source tree

- `PluginProcessor.{h,cpp}` — APVTS, scheduler, DSP glue, save/restore, presets, A/B
- `PluginEditor.{h,cpp}` — scaled JUCE canvas and settings
- `ui/` — theme, TopBar, LoopRow, GlitchKnob, WaveDisplay, GateGrid, ModeRow, BandSlider, PresetBar, LookAndFeel
- `dsp/` — TempoClock, SliceCapture, Reverser, GateSequencer, GateShaper, BandSplitter, Crossfader, TriggerRouter, GlitchRandomizer
- `tests/DSPTests.cpp` — portable regression tests
- `.github/workflows/build.yml` — CI for portable tests and Windows/macOS build targets

**Branding**: Original Skell-Glitch source / original logo art. Reference screenshot is not bundled and no competitor branding is redistributed.
