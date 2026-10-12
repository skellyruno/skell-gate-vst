# Skellgate

A Gross Beat-style time / volume envelope effect (VST3 + Standalone, Windows).

## How it works

A pattern is **2 beats** long and locked to the host tempo and transport.

- **Time lane (green)** - the curve is the playback position inside the audio recorded during the pattern. A diagonal line is normal playback; flat = freeze, falling = reverse, repeated saws = stutter, steeper = faster.
- **Volume lane (orange)** - the curve is the gain: gates, tremolo, fades, ducking.

Each lane has 36 preset pads. Click a pad to load it, then edit the curve:

| Action | Result |
|---|---|
| Drag a node | move it (Shift = no snap) |
| Double-click empty space | add a node |
| Double-click / right-click a node | delete it |
| `1/16` ... `1/3` buttons | snap node positions to that fraction of a beat (click again to turn off) |
| `T` / `V` buttons | choose which lane the graph edits |

## Controls

- **Time / Volume mix knobs** - dry/wet for each lane.
- **ATT / REL** - smoothing of volume rises / falls (0 - 50 ms).
- **TNS** - curve tension between nodes (50 = straight lines).
- **HOLD** - freeze the pattern position.

## MIDI preset switching

Send MIDI notes to the plugin from a MIDI track:

- C1 - B3 (notes 24 - 59) select volume presets 1 - 36
- C4 - B6 (notes 60 - 95) select time presets 1 - 36

## Building

Needs CMake 3.22+ and Visual Studio 2022. JUCE is fetched automatically.

```
cmake -B build
cmake --build build --config Release
```

The VST3 is in `build/Skellgate_artefacts/Release/VST3/`. The GitHub Actions workflow in `.github/workflows/build.yml` does the same and uploads the plugin as an artifact.
