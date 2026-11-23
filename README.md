# OriginPsy

An FX plugin (VST3, AU on macOS, and Standalone) with effects built for psytrance that you won't find as ready-made plugins. Version 0.1 has three modules plus Fraza, a modulation source tied to the phrase.

## How it works

The top row has four **slots**. Pick a module for each one; the sound runs through the slots from left to right. The **ON** button on a slot plays it or skips it. A module plays in one slot only: if you pick it in a second slot, that slot is skipped and the card says so. Clicking a slot opens its module in the middle.

Tempo and bar position come from the DAW. When the DAW is stopped (or in the Standalone version) the plugin runs its own clock at the DAW tempo, or 140 BPM, so the effects keep moving while you tweak them.

### Fraktal Echo

A delay whose echoes branch like a tree. One branch carries on at the same stride, the other is shorter (**Branch**; 0.618 is the golden ratio) and shifted by a number of scale degrees (**Pitch Spread**, in the chosen **Scale**). The tail turns into a melody in key instead of the same sound fading out. The drawing on the left shows the tree: time runs to the right, pitch goes up and down.

- **Time**: stride of the main branch (1/32 to 1 bar, with triplets and dotted values)
- **Generations**: how many times the tree branches (1 to 4, which is 1 to 15 echoes)
- **Feedback**: the whole tree is fed back into the input, so the notes keep climbing or falling
- **Damp** darkens the repeats, **Width** spreads the branches left and right, **Mix**

### Vokal Acid

A formant filter that follows the pitch of an acid or lead line and gives each note (C to B) a vowel from the map (A, E, I, O, U). The line "talks" along with its melody, with no automation.

- **Glide**: how fast the sound slides from one vowel to the next
- **Formant**: shifts all formants up or down (semitones)
- **Resonance**, **Accent** (a sudden jump in level briefly pushes resonance and level up), **Drive** (2x oversampled), **Mix**
- **Character**: Male, Female or Robot (the formants lock onto the harmonics of the note for a metallic sound)

The left side shows which vowel is playing and which note the plugin hears.

### Polimetar Gate

The sound is split into three bands (**Low | Mid** and **Mid | High**), and each band has its own gate rhythm and its own pattern length. With 16, 7 and 13 steps, for example, the rhythms line up again only after many bars, so a single pad becomes a texture that never repeats.

- **Mode**: Euclid (**Hits** spread as evenly as possible over **Steps**, **Rotate** shifts them) or Steps (click the ring to draw steps)
- **Rate**: length of one step
- **Depth**: how far the band is turned down between hits (Low defaults to 0 % so kick and bass stay steady)
- **Length**: how long the gate stays open within a step; **Attack** and **Release** keep it click-free

### Fraza

A curve that knows where you are in an 8, 16 or 32 bar phrase. The knobs on the right set how far the curve moves each parameter (Echo Mix, Echo Feedback, Echo Damp, Vokal Formant, Vokal Resonance, Gate Depth), so the build-up plays itself with no automation lanes. **Offset** moves the start of the phrase by a few bars. The **DROP** button sets up a ready-made build-up over the last 4 bars of the phrase in one click.

## Building

You need CMake 3.22 or newer and a C++20 compiler (Visual Studio 2022 on Windows, Xcode on macOS). JUCE 8.0.9 is downloaded automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The plugin ends up in `build/OriginPsy_artefacts/Release/VST3/OriginPsy.vst3`. With `-DORIGINPSY_COPY_AFTER_BUILD=ON` it is copied into the system plugin folder after building.

GitHub Actions builds Windows and macOS on every push and pull request; download the plugin from the **Actions** tab, under **Artifacts**.

The plugin's vendor is **AMV**, so DAWs list it next to AMVSynth and AMVScope.

## Tests

- `originpsy_dsp_tests`: the JUCE-free DSP core (echo timing, pitch tracker, Euclidean rhythms, polymeter, Fraza, null tests, stability at extreme settings, CPU)
- `originpsy_plugin_tests`: the whole plugin without a DAW (44.1 to 192 kHz, various block sizes, saving state, Drop)
- pluginval at strictness level 5 in GitHub Actions (Windows)
- `originpsy_plugin_tests --screenshot shot.png 1` renders the editor (1 = Echo, 2 = Vokal, 3 = Gate)

The checklist for manual testing in Bitwig is in [docs/TESTING.md](docs/TESTING.md).

## Roadmap

The full plan with all six modules is in [docs/PLAN.md](docs/PLAN.md). Next up: drag and drop modulation and presets, then Zap Rezonator, Orbit Doppler and Spektral Ratchet.
