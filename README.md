# OriginPsy

An FX plugin (VST3, AU on macOS, and Standalone) with effects built for psytrance that you won't find as ready-made plugins. It has six modules plus Phrase, a modulation source tied to the phrase.

## How it works

The top row has four **slots**. Pick a module for each one; the sound runs through the slots from left to right. The **ON** button on a slot plays it or skips it. A module plays in one slot only: if you pick it in a second slot, that slot is skipped and the card says so. Clicking a slot opens its module in the middle.

Tempo and bar position come from the DAW. When the DAW is stopped (or in the Standalone version) the plugin runs its own clock at the DAW tempo, or 140 BPM, so the effects keep moving while you tweak them.

### Fractal Echo

A delay whose echoes branch like a tree. One branch carries on at the same stride, the other is shorter (**Branch**; 0.618 is the golden ratio) and shifted by a number of scale degrees (**Pitch Spread**, in the chosen **Scale**). The tail turns into a melody in key instead of the same sound fading out. The drawing on the left shows the tree: time runs to the right, pitch goes up and down.

- **Time**: stride of the main branch (1/32 to 1 bar, with triplets and dotted values)
- **Generations**: how many times the tree branches (1 to 4, which is 1 to 15 echoes)
- **Feedback**: the whole tree is fed back into the input, so the notes keep climbing or falling
- **Damp** darkens the repeats, **Width** spreads the branches left and right, **Mix**

### Vocal Acid

A formant filter that follows the pitch of an acid or lead line and gives each note (C to B) a vowel from the map (A, E, I, O, U). The line "talks" along with its melody, with no automation.

- **Glide**: how fast the sound slides from one vowel to the next
- **Formant**: shifts all formants up or down (semitones)
- **Resonance**, **Accent** (a sudden jump in level briefly pushes resonance and level up), **Drive** (2x oversampled), **Mix**
- **Character**: Male, Female or Robot (the formants lock onto the harmonics of the note for a metallic sound)

The left side shows which vowel is playing and which note the plugin hears.

### Polymeter Gate

The sound is split into three bands (**Low | Mid** and **Mid | High**), and each band has its own gate rhythm and its own pattern length. With 16, 7 and 13 steps, for example, the rhythms line up again only after many bars, so a single pad becomes a texture that never repeats.

- **Mode**: Euclid (**Hits** spread as evenly as possible over **Steps**, **Rotate** shifts them) or Steps (click the ring to draw steps)
- **Rate**: length of one step
- **Depth**: how far the band is turned down between hits (Low defaults to 0 % so kick and bass stay steady)
- **Length**: how long the gate stays open within a step; **Attack** and **Release** keep it click-free

### Zap Resonator

Every hit on the input (hats, percussion, even a kick) fires a short "laser" zap that starts away from a note and glides onto it, always on a note of the scale. Put it on a hat or percussion track and the groove turns into zaps in the key of the song.

- **Threshold**: how loud a hit must be; **Scale**, **Root**, **Octave**: the notes the zaps land on (two octaves)
- **Note order**: Up, Down, Random, or Hit strength (harder hits go higher)
- **Sweep**: how far away the zap starts (+ falls onto the note, - rises onto it); **Sweep Time**: how fast it lands
- **Decay**: how long it rings; **Resonance**: from noisy (the filtered hit) to a pure tone
- **Mix**: up to 50 % adds the zaps on top, above that the dry sound fades out

### Orbit Doppler

The sound circles the listener along a path (Circle, Figure 8, Spiral, Fly-by), one lap per 1/16 to 16 bars, with real Doppler shift: the pitch rises as it comes closer and falls as it moves away. The far ear hears it quieter and darker, and distance makes it duller.

- **Radius**: size of the path in metres (bigger = faster = more Doppler)
- **Doppler**: 0 = no pitch change, 50 % = real speed of sound, 100 % = twice as strong
- **Distance Tone**: how much darker it gets far away; **Width**: how far left and right it goes

### Tape Stop

Slows the sound down like a tape machine losing power (tempo and pitch fall together), or spins it up from a stop.

- **Mode**: Stop (slows to a halt, comes back at once), Stop & Start (slows down, then spins back up), Slow Down (drops to half speed and back), Start (cuts to a stop and spins up)
- **Trigger**: hold the **BRAKE** button (or automate Tape Brake in the DAW), or let it fire by itself every 2, 4, 8 or 16 bars: the slow down ends exactly on the bar line, and Start spins up right after it
- **Length**: how long the slow down takes (1/16 to 2 bars); **Curve**: falls early and lingers (low) or holds speed and drops late (high)

### Phrase

A curve that knows where you are in an 8, 16 or 32 bar phrase (it rises through the phrase by default; the dot on the curve shows its value now). The knobs on the right set how far the curve moves each parameter (Echo Mix, Echo Feedback, Echo Damp, Vocal Formant, Vocal Resonance, Gate Depth), so the build-up plays itself with no automation lanes. **Offset** moves the start of the phrase by a few bars. The **DROP** button sets up a ready-made build-up over the last 4 bars of the phrase in one click.

### Modulation

The **MOD** row holds the sources: **LFO 1** and **LFO 2** (synced to the song, rate from 1/16 to 16 bars, sine, triangle, saw up, saw down, square or random), **ENV** (follows the input level), the macros **M1** to **M4**, and **PHRASE** (the phrase curve). Each chip shows its live value.

- Drag a source onto any knob to connect it. A coloured ring around the knob shows how far it moves, and a white dot shows where the parameter really is right now.
- Shift-drag the knob to set the depth of its newest connection. Right-click it to choose the direction: **Up** or **Down** (one way from where the knob is) or **Both ways** (swings around the knob). LFOs start as Both ways, the other sources as Up.
- Up to 8 connections at once. They are saved with the project and in presets.
- The macros start wired the same way in every factory preset: M1 Space (echo mix and feedback), M2 Talk (vokal resonance), M3 Chop (gate depth on the mid and high bands). M4 is free.

### MIDI control

OriginPsy listens to MIDI on any channel, so pads or clips can play the performance buttons:

| MIDI | Does |
| --- | --- |
| Note C1 (36), held | **BRAKE** (Tape Stop slows down while held, comes back when let go) |
| Note D1 (38) | **DROP** |
| Note E1 (40) | Previous preset |
| Note F1 (41) | Next preset |
| Program Change n | Loads preset n of the list (0 = first factory preset; user presets follow the factory ones) |

In Bitwig, put OriginPsy on an instrument track after the instrument, or use a Note Receiver on the audio track, and send it notes from a clip or controller. "MIDI" flashes in the header when a message does something.

### Presets

The bar at the top lists factory presets by category (Init, Build-up, Lead FX, Bass, Pads, Glitch), then your own. **Save** stores the whole state (parameters, drawn gate steps and modulation) as a `.originpsy` file in `Documents/AMV/OriginPsy Presets`. Subfolders there show up too.

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

- `originpsy_dsp_tests`: the JUCE-free DSP core (echo timing, pitch tracker, LFOs, modulation matrix, envelope follower, Euclidean rhythms, polymeter, Phrase, null tests, stability at extreme settings, CPU)
- `originpsy_plugin_tests`: the whole plugin without a DAW (44.1 to 192 kHz, various block sizes, saving state, Drop, modulation, loading every factory preset, saving and loading a user preset)
- pluginval at strictness level 5 in GitHub Actions (Windows)
- `originpsy_plugin_tests --screenshot shot.png 1` renders the editor (1 = Echo, 2 = Vocal, 3 = Gate, 4 = Zap, 5 = Orbit, 6 = Tape)

The checklist for manual testing in Bitwig is in [docs/TESTING.md](docs/TESTING.md).

## Roadmap

The full plan with all six modules is in [docs/PLAN.md](docs/PLAN.md). Next up: Spectral Ratchet.
