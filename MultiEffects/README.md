# Multi Effects

Four effects in a single sketch for Fantasia, built with the Teensy Audio
Library. Press a button to select the effect, use the four knobs to edit
its parameters.

## Buttons (effect select)

| Button | Pin | Effect | Class |
|:------:|:---:|--------|-------|
| 1 | 2 | Reverb | `AudioEffectFreeverb` |
| 2 | 5 | Chorus | `AudioEffectChorus` |
| 3 | 25 | Delay | `AudioEffectDelay` (with feedback) |
| 4 | 27 | Granular | `AudioEffectGranular` |

Buttons latch: pressing one engages its effect and it stays on until a
different button is pressed. On boot no effect is engaged, only the dry
signal passes.

While the granular effect is engaged, pressing button 4 again toggles
between **Pitch Shift** and **Freeze** modes.

## Knob parameters

Knobs 1 and 2 are effect parameters, they do something different on every
effect. Knobs 3 and 4 are the wet/dry mix and always work the same way.

| Knob | Pin | Reverb | Chorus | Delay | Granular |
|:----:|:---:|--------|--------|-------|----------|
| 1 | A0 | Room size (0–100 %) | Voices (1–4) | Delay time (10–1000 ms) | Grain length (10–190 ms) |
| 2 | A1 | Damping (0–100 %) | Depth (0.7–46 ms) | Feedback (0–90 %) | Speed (0.5×–2.0×) |
| 3 | A2 | Wet level (0–100 %) | Wet level (0–100 %) | Wet level (0–100 %) | Wet level (0–100 %) |
| 4 | A3 | Dry level (0–100 %) | Dry level (0–100 %) | Dry level (0–100 %) | Dry level (0–100 %) |

### Reverb (`AudioEffectFreeverb`)

* **Knob 1 – Room size**: size of the simulated room. 0 % = small room with
  a short tail, 100 % = huge hall with a very long tail (at maximum it can
  ring a little).
* **Knob 2 – Damping**: how fast the high frequencies decay in the tail.
  0 % = bright, open tail. 100 % = dark, fast decay.
* The reverb class is mono, the result is copied to both outputs.

### Chorus (`AudioEffectChorus`)

* **Knob 1 – Voices**: number of chorus voices including the original.
  `1` = dry passthrough (effect off), `2` = classic chorus, `3` / `4` =
  thicker.
* **Knob 2 – Depth**: delay used by the chorus voices, from 0.7 ms
  (flanger-ish, metallic) to 46 ms (wide chorus / doubling). Only audible
  with more than one voice.
* Note: this chorus has no LFO, the voices are static delays. That is why
  the sketch calls `begin()` again when the knobs change.

### Delay (`AudioEffectDelay`)

* **Knob 1 – Delay time**: 10 ms to 1000 ms.
* **Knob 2 – Feedback**: 0–90 %. How much of the echo is fed back into the
  delay so the repeats regenerate. Keep it under ~90 % if you don't want it
  to self-oscillate.
* The delay only holds its memory blocks while it is engaged, leaving the
  effect frees them again.
* Memory: each ms of delay holds about one block of `AudioMemory()`, see
  the notes below.

### Granular (`AudioEffectGranular`)

* **Knob 1 – Grain length**: 10–190 ms, used by both modes.
* **Knob 2 – Speed**: playback rate of the grains, 0.5×–2.0× (one octave
  down to one octave up, also transposes the frozen loop).
* **Button 4 pressed again toggles the modes**:
  * *Pitch Shift*: continuously records new grains and plays them back at
    the speed of knob 2.
  * *Freeze*: captures a single grain and loops it forever.

### Wet / dry (all effects)

* **Knob 3 – Wet**: level of the processed signal.
* **Knob 4 – Dry**: level of the direct signal. With no effect engaged this
  is the only signal that passes, so it works as a bypass level.
* Keep wet + dry ≲ 100 % (e.g. 70 % + 70 %) to avoid clipping the output.

## Signal flow

```
line in (left) ──┬──> reverb   ──┐
                  ├──> chorus   ──┤
                  ├──> delay    ──┼──> wet (knob 3) ──┐
                  ├──> granular ──┘                    ├──> out L / out R
                  └──> dry (knob 4) ───────────────────┘
```

The sketch processes the left channel of the line input and sends the
result to both outputs.

## Pins used

| Function | Pin |
|----------|-----|
| Knobs 1–4 | A0, A1, A2, A3 |
| Buttons 1–4 | 2, 5, 25, 27 (INPUT_PULLUP, pressed = LOW) |
| Line in / line out | I2S audio shield (SGTL5000) |

## Notes

* Dependencies: Teensy Audio Library and Bounce, both come with Teensyduino.
* `AudioMemory(400)` ≈ 105 KB of RAM for the audio block pool, sized for
  the 1000 ms maximum delay. The sketch is tuned for Teensy 4.x (like the
  rest of the Fantasia firmware). On a Teensy 3.x lower `MAX_DELAY_MS`
  (e.g. 400) and `AudioMemory()` (e.g. 160) in the sketch.
* The SD card is not used by this sketch.
