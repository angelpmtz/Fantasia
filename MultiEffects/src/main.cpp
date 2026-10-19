/*
  MultiEffects - four selectable effects in one sketch for Fantasia

  Press a button to engage an effect (buttons latch: the effect stays
  engaged until a different button is pressed):
    Button 1 = Reverb   (AudioEffectFreeverb)
    Button 2 = Chorus   (AudioEffectChorus)
    Button 3 = Delay    (AudioEffectDelay with feedback)
    Button 4 = Granular (AudioEffectGranular)

  Pressing Button 4 again while the granular effect is engaged toggles
  between Pitch Shift and Freeze modes.

  Knobs (see README.md for the complete parameter table):
    POT/CV 1 = effect parameter 1 (different on every effect)
    POT/CV 2 = effect parameter 2 (different on every effect)
    POT/CV 3 = wet level
    POT/CV 4 = dry level

  On boot no effect is engaged: only the dry signal (POT/CV 4) passes.

  Display: turning a knob shows its value (0-10) on the 7 segment display,
  10 is shown as "9." (nine with the decimal point). After 2 seconds
  without touching a knob the display shows a spinning animation.
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Bounce.h>
#include "SegmentDisplay.h" /* Seven Segment Library by Derek Duncan */

//                             E   D   C  DP  B   A   G   F
//                             1   2   4  5   6   7   9  10
SegmentDisplay segmentDisplay(31, 28, 33, 9, 32, 30, 26, 29);

// Chorus delay line: 4096 samples -> chorus taps from 0.7 ms to 46 ms
#define CHORUS_DELAY_LENGTH (32 * AUDIO_BLOCK_SAMPLES)
// Granular sample bank: 25600 samples -> ~580 ms at 44.1 kHz
#define GRANULAR_MEMORY_SIZE 25600
// How long a new grain length keeps being sent to the granular effect
#define GRANULAR_APPLY_MS 60
// Delay range, watch AudioMemory() when you raise MAX_DELAY_MS (see README)
#define MIN_DELAY_MS 10
#define MAX_DELAY_MS 1000

// Sample buffers used by chorus and granular
short chorusDelayline[CHORUS_DELAY_LENGTH];
int16_t granularMemory[GRANULAR_MEMORY_SIZE];

// GUItool: begin automatically generated code
AudioInputI2S i2s1;            // line in
AudioEffectFreeverb freeverb1; // button 1
AudioEffectChorus chorus1;     // button 2
AudioMixer4 delayInputMixer;   // dry input + feedback for the delay
AudioEffectDelay delay1;       // button 3
AudioEffectGranular granular1; // button 4
AudioMixer4 fxMixer;           // effect selector
AudioAmplifier wetAmp;         // POT/CV 3
AudioAmplifier dryAmp;         // POT/CV 4
AudioMixer4 outMixer;          // wet + dry sum
AudioOutputI2S i2s2;           // line out
AudioConnection patchCord1(i2s1, 0, freeverb1, 0);
AudioConnection patchCord2(i2s1, 0, chorus1, 0);
AudioConnection patchCord3(i2s1, 0, delayInputMixer, 0);
AudioConnection patchCord4(i2s1, 0, granular1, 0);
AudioConnection patchCord5(i2s1, 0, dryAmp, 0);
AudioConnection patchCord6(delayInputMixer, 0, delay1, 0);
AudioConnection patchCord7(delay1, 0, delayInputMixer, 1); // feedback loop
AudioConnection patchCord8(delay1, 0, fxMixer, 2);
AudioConnection patchCord9(freeverb1, 0, fxMixer, 0);
AudioConnection patchCord10(chorus1, 0, fxMixer, 1);
AudioConnection patchCord11(granular1, 0, fxMixer, 3);
AudioConnection patchCord12(fxMixer, 0, wetAmp, 0);
AudioConnection patchCord13(wetAmp, 0, outMixer, 0);
AudioConnection patchCord14(dryAmp, 0, outMixer, 1);
AudioConnection patchCord15(outMixer, 0, i2s2, 0);
AudioConnection patchCord16(outMixer, 0, i2s2, 1);
AudioControlSGTL5000 sgtl5000_1;
// GUItool: end automatically generated code

// SD CARD PINS (not used in this sketch)
#define SDCARD_CS_PIN 10
#define SDCARD_MOSI_PIN 11
#define SDCARD_SCK_PIN 13

// select the input pins for the potentiometers
int PotPort1 = A0;
int PotPort2 = A1;
int PotPort3 = A2;
int PotPort4 = A3;

// pushbutton connected to digital pins
int Boton1 = 2;
int Boton2 = 5;
int Boton3 = 25;
int Boton4 = 27;

// debounced buttons (Bounce comes with Teensyduino)
Bounce boton1 = Bounce(Boton1, 15);
Bounce boton2 = Bounce(Boton2, 15);
Bounce boton3 = Bounce(Boton3, 15);
Bounce boton4 = Bounce(Boton4, 15);

// normalized pot values, 0.0 to 1.0
float PotNorm1 = 0;
float PotNorm2 = 0;
float PotNorm3 = 0;
float PotNorm4 = 0;

// selected effect, the value - 1 is the fxMixer input it is routed to
#define FX_NONE 0
#define FX_REVERB 1
#define FX_CHORUS 2
#define FX_DELAY 3
#define FX_GRANULAR 4
int currentFx = FX_NONE;

// granular sub modes
#define GRANULAR_PITCHSHIFT 0
#define GRANULAR_FREEZE 1
int granularMode = GRANULAR_PITCHSHIFT;
int granularMs = -1;
unsigned long granularBurstStart = 0;

// last values applied to the effects, so they are only restarted when a
// knob really moved and not on every loop pass
int lastChorusVoices = 2;
int lastChorusDepth = CHORUS_DELAY_LENGTH;
int lastDelayMs = -1;

// 7 segment display: shows the value of the knob that is being turned on a
// 0..10 scale (the display has a single digit, so 10 is shown as "9." with
// the decimal point). After DISPLAY_IDLE_MS without knob movement the
// display runs a spinning animation until a knob is touched again.
#define DISPLAY_IDLE_MS 2000
#define DISPLAY_SPIN_MS 80
int displayValue = -1;               // digit currently shown, -1 = nothing yet
int displayKnob = -1;                // knob whose value is shown (0..3)
int lastQuant[4] = {-1, -1, -1, -1}; // last 0..10 step of every knob
unsigned long lastKnobMoveMs = 0;
bool displaySleeping = false;
int spinStep = 0;
unsigned long lastSpinMs = 0;

// same pins as segmentDisplay, needed to drive the spin animation directly
// (the library has no per-segment API). Common anode: LOW = segment lit.
//                             E   D   C  DP  B   A   G   F
byte displayPins[8] = {31, 28, 33, 9, 32, 30, 26, 29};
// spin order: A B C D E F G DP, going clockwise around the display
const byte spinOrder[8] = {5, 4, 2, 1, 0, 7, 6, 3};

// prototypes (the Arduino IDE generates these automatically, PlatformIO does not)
void applyGranularLength(bool restart);

const char *effectName(int fx)
{
  switch (fx)
  {
  case FX_REVERB:
    return "Reverb";
  case FX_CHORUS:
    return "Chorus";
  case FX_DELAY:
    return "Delay";
  case FX_GRANULAR:
    return "Granular";
  default:
    return "Dry (no effect)";
  }
}

// engage an effect, everything else on the selector is muted
void selectEffect(int fx)
{
  if (fx == currentFx)
    return;

  // release what the effect being left was using
  if (currentFx == FX_DELAY)
  {
    delayInputMixer.gain(1, 0); // kill the feedback loop
    delay1.disable(0);          // free the delay memory blocks
  }
  if (currentFx == FX_GRANULAR)
    granular1.stop();

  // mute every effect on the selector
  fxMixer.gain(0, 0);
  fxMixer.gain(1, 0);
  fxMixer.gain(2, 0);
  fxMixer.gain(3, 0);

  currentFx = fx;

  if (fx != FX_NONE)
    fxMixer.gain(fx - 1, 1.0);
  Serial.print("Effect: ");
  Serial.println(effectName(fx));

  if (fx == FX_DELAY)
    lastDelayMs = -1; // re-apply (and re-enable) the delay
  if (fx == FX_GRANULAR)
  {
    granularMode = GRANULAR_PITCHSHIFT;
    applyGranularLength(true);
    Serial.println("Granular mode: pitch shift (press button 4 again for freeze)");
  }
}

// apply the grain length (POT/CV 1) to the granular effect
void applyGranularLength(bool restart)
{
  int ms = 10 + (int)(PotNorm1 * 180.0); // 10 .. 190 ms
  // 3 ms of hysteresis so pot noise doesn't restart the grain for nothing
  if (restart || ms - granularMs >= 3 || granularMs - ms >= 3)
  {
    granularMs = ms;
    granularBurstStart = millis();
  }
  // the library only latches a new grain length while it is ready to record
  // a grain, so keep sending the value for a few ms until it sticks
  if (millis() - granularBurstStart < GRANULAR_APPLY_MS)
  {
    if (granularMode == GRANULAR_FREEZE)
      granular1.beginFreeze(granularMs);
    else
      granular1.beginPitchShift(granularMs);
  }
}

// show a 0..10 knob value on the 7 segment display, 10 is shown as "9."
void displayKnobValue(int knob, int value)
{
  if (value == displayValue && knob == displayKnob)
    return;
  displayValue = value;
  displayKnob = knob;
  if (value >= 10)
    segmentDisplay.displayHex(9, true);
  else
    segmentDisplay.displayHex(value, false);
}

// call every pass of loop(): shows the value of the knob that is moving and
// after 2 seconds without movement switches to the spinning animation
void updateDisplay()
{
  float pots[4] = {PotNorm1, PotNorm2, PotNorm3, PotNorm4};
  int moved = -1;

  // quantize to 0..10, a step is ~51 ADC counts so pot noise doesn't flicker
  for (int i = 0; i < 4; i++)
  {
    int value = (int)(pots[i] * 10.0f + 0.5f);
    if (value != lastQuant[i])
    {
      lastQuant[i] = value;
      moved = i;
    }
  }

  unsigned long now = millis();

  if (moved >= 0)
  {
    // a knob crossed a full step: show its value
    displayKnobValue(moved, lastQuant[moved]);
    lastKnobMoveMs = now;
    displaySleeping = false;
    return;
  }

  // no knob movement: after 2 s idle, spin the segments one at a time
  if (!displaySleeping && now - lastKnobMoveMs >= DISPLAY_IDLE_MS)
  {
    displaySleeping = true;
    spinStep = 0;
    lastSpinMs = now;
  }

  if (displaySleeping && now - lastSpinMs >= DISPLAY_SPIN_MS)
  {
    lastSpinMs = now;
    for (int s = 0; s < 8; s++)
    {
      digitalWrite(displayPins[s], HIGH); // all off (common anode)
    }
    digitalWrite(displayPins[spinOrder[spinStep]], LOW); // one lit
    spinStep = (spinStep + 1) % 8;
  }
}

void setup()
{

  // Audio setup
  AudioMemory(400);
  sgtl5000_1.enable();
  sgtl5000_1.volume(1);
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_1.micGain(36); // NEEDED?

  // open the serial port at 9600 bps:
  Serial.begin(9600);
  Serial.println("MultiEffects: buttons = Reverb / Chorus / Delay / Granular");

  // sets the digital pins as inputs and set pullups
  pinMode(Boton1, INPUT_PULLUP);
  pinMode(Boton2, INPUT_PULLUP);
  pinMode(Boton3, INPUT_PULLUP);
  pinMode(Boton4, INPUT_PULLUP);

  // the SegmentDisplay constructor already configures the display pins
  // (30, 32, 33, 28, 31, 26, 29, 9)

  // effects that need their buffers set up
  chorus1.begin(chorusDelayline, CHORUS_DELAY_LENGTH, lastChorusVoices);
  granular1.begin(granularMemory, GRANULAR_MEMORY_SIZE);

  // selector: every effect muted, no effect engaged at boot
  fxMixer.gain(0, 0);
  fxMixer.gain(1, 0);
  fxMixer.gain(2, 0);
  fxMixer.gain(3, 0);

  // the dry input always feeds the delay, feedback only while it is engaged
  delayInputMixer.gain(0, 1.0);
  delayInputMixer.gain(1, 0);

  // wet / dry sum
  outMixer.gain(0, 1.0);
  outMixer.gain(1, 1.0);
  outMixer.gain(2, 0);
  outMixer.gain(3, 0);

  // seed the display tracking with the current knob positions so it starts
  // with the spinning animation instead of showing a value nobody turned
  float bootPots[4] = {(float)analogRead(PotPort1) / 1023.0f,
                       (float)analogRead(PotPort2) / 1023.0f,
                       (float)analogRead(PotPort3) / 1023.0f,
                       (float)analogRead(PotPort4) / 1023.0f};
  for (int i = 0; i < 4; i++)
    lastQuant[i] = (int)(bootPots[i] * 10.0f + 0.5f);
  lastKnobMoveMs = millis() - DISPLAY_IDLE_MS; // begin spinning right away
}

void loop()
{

  // read the value from the pots, normalized to 0.0 .. 1.0
  PotNorm1 = (float)analogRead(PotPort1) / 1023.0;
  PotNorm2 = (float)analogRead(PotPort2) / 1023.0;
  PotNorm3 = (float)analogRead(PotPort3) / 1023.0;
  PotNorm4 = (float)analogRead(PotPort4) / 1023.0;

  // 7 segment display: knob value while turning, spin animation when idle
  updateDisplay();

  // read the buttons, pressing one engages its effect
  boton1.update();
  boton2.update();
  boton3.update();
  boton4.update();

  if (boton1.fallingEdge())
    selectEffect(FX_REVERB);
  if (boton2.fallingEdge())
    selectEffect(FX_CHORUS);
  if (boton3.fallingEdge())
    selectEffect(FX_DELAY);
  if (boton4.fallingEdge())
  {
    if (currentFx != FX_GRANULAR)
    {
      selectEffect(FX_GRANULAR); // first press: pitch shift mode
    }
    else
    {
      // already on granular: toggle between pitch shift and freeze
      granularMode = (granularMode == GRANULAR_PITCHSHIFT) ? GRANULAR_FREEZE : GRANULAR_PITCHSHIFT;
      applyGranularLength(true);
      Serial.println(granularMode == GRANULAR_FREEZE ? "Granular mode: freeze" : "Granular mode: pitch shift");
    }
  }

  // wet / dry, the same on every effect
  wetAmp.gain(PotNorm3);
  dryAmp.gain(PotNorm4);

  // parameters of the engaged effect
  switch (currentFx)
  {

  case FX_REVERB:
    freeverb1.roomsize(PotNorm1); // 0.0 .. 1.0
    freeverb1.damping(PotNorm2);  // 0.0 .. 1.0
    break;

  case FX_CHORUS:
  {
    // voices: 1 = dry passthrough, 2 .. 4 = chorus voices
    float pos = PotNorm1 * 4.0;
    int voices = 1 + (int)pos;
    if (voices > 4)
      voices = 4;
    bool chorusChanged = false;

    // hysteresis so pot noise on a band edge doesn't restart the chorus
    if (voices != lastChorusVoices)
    {
      float margin = 0.15;
      bool past = (voices > lastChorusVoices) ? (pos >= lastChorusVoices + margin)
                                              : (pos <= voices - margin);
      if (past)
      {
        lastChorusVoices = voices;
        chorusChanged = true;
      }
    }

    // depth: chorus tap delay, 64 .. 4096 samples = 0.7 .. 46 ms
    int depth = 64 + (int)(PotNorm2 * (CHORUS_DELAY_LENGTH - 64));
    // 16 samples of hysteresis (pot noise reads as less than that)
    if (depth - lastChorusDepth >= 16 || lastChorusDepth - depth >= 16)
    {
      lastChorusDepth = depth;
      chorusChanged = true;
    }

    if (chorusChanged)
    {
      // begin() only resets pointers and counters, guard it so the audio
      // interrupt cannot run in the middle of it
      noInterrupts();
      chorus1.begin(chorusDelayline, lastChorusDepth, lastChorusVoices);
      interrupts();
    }
    break;
  }

  case FX_DELAY:
  {
    int ms = MIN_DELAY_MS + (int)(PotNorm1 * (MAX_DELAY_MS - MIN_DELAY_MS));
    // 4 ms of hysteresis so pot noise doesn't jump the delay time around
    if (ms - lastDelayMs >= 4 || lastDelayMs - ms >= 4)
    {
      delay1.delay(0, ms);
      lastDelayMs = ms;
    }
    delayInputMixer.gain(1, PotNorm2 * 0.9); // feedback 0 .. 90 %
    break;
  }

  case FX_GRANULAR:
    applyGranularLength(false);
    granular1.setSpeed(powf(2.0, PotNorm2 * 2.0 - 1.0)); // 0.5x .. 2.0x
    break;
  }
}
