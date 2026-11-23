# Testing in Bitwig

For each item: does it sound good, are there clicks, does it follow the tempo? Note what is wrong and at which tempo.

## General

- [ ] Drag a slot card onto another: the two swap, the sound follows the new order without a click

- [ ] The VST3 loads on an audio track and opens and closes without problems
- [ ] The window can be resized by dragging the corner
- [ ] A saved and reopened project restores every setting, including drawn gate steps
- [ ] Changing a slot's module or switching it ON/OFF makes no click
- [ ] Tempo changes in Bitwig (138, 145, 150) and 7/8: the effects stay in time
- [ ] CPU in Bitwig with all three modules on

## Fractal Echo

- [ ] On a single hi-hat or zap: the first echo lands exactly on the chosen length (1/8, 1/16D...)
- [ ] Pitch Spread 0: a plain echo with no pitch change; 2 or 3: the tail climbs the scale
- [ ] Negative Pitch Spread: the tail falls
- [ ] Feedback all the way up: the tail lasts long but does not blow up
- [ ] Scale: Phrygian, Hijaz, Harmonic Minor sound in key with the lead

## Vocal Acid

- [ ] On a 303 line: the display shows the right note and the vowel changes with the note
- [ ] Short (10 ms) and long (300 ms) Glide
- [ ] Accent: accented notes stand out
- [ ] Robot: metallic sound, no crackling
- [ ] On a bass (low notes, 40 to 80 Hz) it recognises the notes

## Polymeter Gate

- [ ] Default (16, 7, 13): a pad becomes a moving texture, kick and bass (Low) stay steady
- [ ] Steps mode: clicking the ring adds and removes steps
- [ ] Rate 1/16T and 1/8D
- [ ] Release 0.1 ms: hard chopping; does it click?
- [ ] Mix 50 %: no hollow, phasey sound

## Phrase

- [ ] The position (bar X / 16) matches Bitwig
- [ ] DROP: over the last 4 bars of the phrase the echo grows and the gate deepens; on beat 1 of the next phrase everything returns
- [ ] Offset: the phrase start moves when the song does not start on bar 1

## Zap Resonator

- [ ] On a hi-hat loop: every hat fires a zap, and the zaps are in the key (Scale, Root)
- [ ] Threshold up: only the loud hits fire; Note order Random and Hit strength sound different from Up
- [ ] Sweep +24 falls onto the note, -24 rises onto it; Resonance 0 % sounds noisy, 100 % a clean tone
- [ ] Mix 50 %: hats plus zaps; 100 %: only zaps

## Orbit Doppler

- [ ] On a lead: the sound goes round the head in time with the song (One lap = 1 bar)
- [ ] The pitch bends up as it comes closer and down as it moves away; Doppler 0 % keeps the pitch
- [ ] Fly-by: passes in front from left to right and back; Spiral: swings in close and out
- [ ] No clicks when changing Path or One lap while playing

## Tape Stop

- [ ] Hold BRAKE: the sound slows down to a stop over the Length; let go: it is back at once (Stop) or spins up (Stop & Start)
- [ ] Slow Down: holds an octave lower while BRAKE is held, then speeds back up
- [ ] Trigger Every 4 bars: the stop ends exactly on the bar line and the next bar starts clean
- [ ] Start with Every 4 bars: each 4 bar block starts with a spin-up
- [ ] Automating Tape Brake in Bitwig works like holding the button
- [ ] No clicks going in or out

## Spectral Ratchet

- [ ] On a lead or pad: freeze steps hold the sound, pass steps let it through, in time with the song
- [ ] Interval Scale up: each repeat climbs the scale; Octaves up: climbs in octaves
- [ ] Mask Alternate combs and Walking band change the colour on each repeat
- [ ] Adding it to a slot: Bitwig compensates the latency (the track stays in time with the others)
- [ ] Mix 0 %: sounds exactly like the dry track

## Modulation

- [ ] Drag LFO 1 onto Echo Feedback: a blue ring appears and the white dot moves with the LFO in time with the song
- [ ] Shift-drag the knob up and down: the ring grows and shrinks; right-click: Up, Down and Both ways change the direction (the ring follows), Remove deletes it
- [ ] Drag PHRASE onto a knob: the dot on the Phrase curve and the white dot on the knob climb together through the phrase
- [ ] ENV on Vocal Formant: the formant follows how loud the input is
- [ ] M1 (Space) turned up: more echo; M3 (Chop): the gate gets deeper
- [ ] Bitwig automation of a modulated knob still works, and the modulation rides on top of it
- [ ] Save the Bitwig project, reopen it: connections and depths are back

## Randomize and Mutate

- [ ] Randomize a few times: each result sounds different and usable, nothing blows up
- [ ] Mutate: small changes around the current sound
- [ ] Modules that are not in the chain keep their settings
- [ ] Ctrl+Z in Bitwig undoes the change (the parameters are host automatable)

## Presets

- [ ] Click through all factory presets with < and >: each one sounds different and nothing clicks or blows up
- [ ] Save a preset, load another, load yours again: everything (gate steps and modulation too) comes back
- [ ] The file is in Documents/AMV/OriginPsy Presets
- [ ] Reopening the project shows the preset name in the bar
