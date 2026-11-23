# Plan

OriginPsy is a chain of four slots that each take one of six modules, plus Phrase, a modulation source that knows where you are in an 8, 16 or 32 bar phrase.

| Module | What it does | Status |
| --- | --- | --- |
| Fractal Echo | A delay whose echoes branch like a tree, each branch shifted by a scale degree | done (v0.1) |
| Vocal Acid | A formant filter whose vowel follows the note the acid line plays | done (v0.1) |
| Polymeter Gate | Three bands, each with its own gate rhythm and length (e.g. 16, 7, 13 steps) | done (v0.1) |
| Phrase + Drop | A build-up tied to the position in the phrase, with no automation | done (v0.1) |
| Zap Resonator | Every hit on the input triggers a short "laser" zap on a note from the scale | done |
| Orbit Doppler | The sound circles the listener along a drawn path, with real Doppler shift (drawn paths later) | done |
| Spectral Ratchet | Freezes the spectrum on each step and repeats it with a different interval or mask | later |

## Phases

1. Skeleton, 4 slot chain, host tempo, CTest and GitHub Actions (done)
2. Fractal Echo (done)
3. Vocal Acid (done)
4. Polymeter Gate (done)
5. Phrase and Drop (done); drag and drop modulation and presets (done)
6. Zap Resonator and Orbit Doppler (done); tape stop / tape start (next, requested in testing)
7. Spectral Ratchet (FFT, latency reported to the host)
8. Final pass: CPU, presets for every module, manual, AU build

## DSP rules

- No memory allocation or locks in `processBlock`; everything is prepared in `prepareToPlay`.
- The DSP core (`src/dsp`) is plain C++ without JUCE and is tested without a DAW.
- Parameters are smoothed, and slot changes go through a short fade.
- Oversampling only where there is a nonlinearity (Drive in Vocal Acid).
