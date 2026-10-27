# Skellgate

A Gross Beat-style time / volume envelope effect (VST3 + Standalone, Windows).
Not affiliated with Image-Line or FL Studio.

## How it works

A pattern is **2 beats** long and locked to the host tempo and transport (free-running at the host tempo, or 120 BPM, when the host is stopped or absent).

- **Time lane (green)** - the curve is the playback position inside the audio recorded during the pattern. A diagonal line is normal playback; flat = freeze, falling = reverse, repeated saws = stutter, steeper = faster.
- **Volume lane (orange)** - the curve is the gain: gates, tremolo, fades, ducking.

Each lane has 36 preset pads. Click a pad to load it, then edit the curve.

| Action | Result |
|---|---|
| Drag a node | move it (Shift = no snap) |
| Double-click empty space | add a node |
| Double-click / right-click a node | delete it |
| `1/16` ... `1/3` buttons | snap node positions to that fraction of a beat (click again to turn off) |
| `T` / `V` buttons | choose which lane the graph edits |

The window is resizable (860 x 650 up to 1600 x 1000).

## Controls

- **Time / Volume mix knobs** - dry/wet for each lane.
- **ATT / REL** - smoothing of volume rises / falls (0.2 ms - 50 ms).
- **TNS** - curve tension between nodes (50 = straight lines; the graph draws the bend).
- **OFFSET** - shifts the whole pattern against the beat.
- **HOLD** - freeze the pattern position.

Time jumps (stutters, slice changes) are crossfaded over 2 ms so they don't click.

## MIDI preset switching

Send MIDI notes to the plugin from a MIDI track:

- C1 - B3 (notes 24 - 59) select volume presets 1 - 36
- C4 - B6 (notes 60 - 95) select time presets 1 - 36

Switches take effect at the start of the audio block the note arrives in.

## Building

Needs CMake 3.22+ and Visual Studio 2022. JUCE 8.0.8 is fetched automatically.

```
cmake -B build
cmake --build build --config Release
```

The VST3 is in `build/Skellgate_artefacts/Release/VST3/`.

### Tests

```
cmake -B build -DSKELLGATE_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`Tests/DspTests.cpp` runs the processor headlessly and checks: exact passthrough with empty presets, stutter and reverse against analytic signals, gate duty cycle, click-free jumps, finite output for every time x volume preset, node-editing rules, state save/load, and CPU use.

### CI

`.github/workflows/build.yml` builds, runs the tests, runs pluginval on the VST3 (advisory) and uploads zips. Push a tag like `v1.1.0` to attach the zips to a GitHub release.

## Licensing

Skellgate links JUCE, which is dual-licensed (AGPLv3 or a commercial licence). Distributing the plugin as closed source needs a JUCE commercial licence.

## Preset notes

The 72 presets are original recreations that match the pad names. They are not Image-Line's data, so they will not sound identical to the real ones, and they are meant to be edited.
