// Auduino Sequencer, the Lo-Fi granular 8-step sequencer
// Modified by NPoole @ SparkFun Electronics ( http://sparkfun.com ) to sequence
// Remodified by Joshua A.C. Newman of the glyphpress (http://glyphpress.com) to work with multiplexers
// Based on the Auduino Synthesizer (v5) by Peter Knight, tinker.it ( http://tinker.it )
//
// Auduino Info: http://code.google.com/p/tinkerit/wiki/Auduino
// Sequencer Hardware Tutorial: https://learn.sparkfun.com/tutorials/step-sequencing-with-auduino
//
// Enjoy!

/*  I've modified this to work with multiplexers instead of an Arduino Mega.
    That allows you to use a Nano, Pro Mini, Pro Micro, or other board with the
    very inexpensive addition of 8-bit 4051 multiplexer ICs.
    It's cheaper, smaller, and much more flexible this way.
    |ζ• (Joshua A.C. Newman)
*/

#include <avr/io.h>
#include <avr/interrupt.h>

uint16_t syncPhaseAcc;
uint16_t syncPhaseInc;
uint16_t grainPhaseAcc;
uint16_t grainPhaseInc;
uint16_t grain1Amp;
uint8_t grain1Decay;
uint16_t grain2PhaseAcc;
uint16_t grain2PhaseInc;
uint16_t grain2Amp;
uint8_t grain2Decay;

/* Set up multiplexers*/

//Select Arduino pins for the 3-bit multiplexer pin selector. For step LEDs.
#define LEDS0 7
#define LEDS1 8
#define LEDS2 9

//Select Arduino pins for the 3-bit multiplexer pin selector. For pots.
#define stepInputS0 10
#define stepInputS1 11
#define stepInputS2 12

//Select Arduino pins for the 3-bit multiplexer pin selector. For step buttons.
#define toneInputS0 10
#define toneInputS1 11
#define toneInputS2 12

//Address bits of the multiplexer pins
int bit0 = 0;      //value of bit 001 to address the 4051 (S0)
int bit1 = 0;      //value of bit 010 to address the 4051 (S1)
int bit2 = 0;      //value of bit 100 to address the 4051 (S2)
//Which pin to read from the multiplexer from 0-7
int whichMuxPin;


// Name the multiplexers by their function and assign their pins
#define potMux        A0
#define stepButtonMux 5
#define stepLEDMux    6

/*  The function of each pot.
    The first 8 are on potMux pins 0-7 and are called using the mux() function so you can just analogRead(them).
    The last 3 are on A1-A3. (A0 is taken by the multiplexer itself).
*/

#define grain1PhasePin     toneInputMux(potMux,0)
#define grain1DecayPin     toneInputMux(potMux,1)
#define grain2PhasePin     toneInputMux(potMux,2)
#define grain2DecayPin     toneInputMux(potMux,3)
#define syncPhasePin       toneInputMux(potMux,4)
#define global1PhasePin    toneInputMux(potMux,5)
#define global1DecayPin    toneInputMux(potMux,6)
#define global2PhasePin    toneInputMux(potMux,7)
#define global2DecayPin    A1
#define globalPhaseFreqPin A2
#define stepTempoPin       A3


//Allocating each step button across the second multiplexer
#define stepButton0Pin  stepInputMux(stepButtonMux,0)
#define stepButton1Pin  stepInputMux(stepButtonMux,1)
#define stepButton2Pin  stepInputMux(stepButtonMux,2)
#define stepButton3Pin  stepInputMux(stepButtonMux,3)
#define stepButton4Pin  stepInputMux(stepButtonMux,4)
#define stepButton5Pin  stepInputMux(stepButtonMux,5)
#define stepButton6Pin  stepInputMux(stepButtonMux,6)
#define stepButton7Pin  stepInputMux(stepButtonMux,7)
#define goButton  A7

//  Allocating each step LED across the third multiplexer
#define stepLED0Pin  stepMux(stepLEDMux,0)
#define stepLED1Pin  stepMux(stepLEDMux,1)
#define stepLED2Pin  stepMux(stepLEDMux,2)
#define stepLED3Pin  stepMux(stepLEDMux,3)
#define stepLED4Pin  stepMux(stepLEDMux,4)
#define stepLED5Pin  stepMux(stepLEDMux,5)
#define stepLED6Pin  stepMux(stepLEDMux,6)
#define stepLED7Pin  stepMux(stepLEDMux,7)


// Changing these will also requires rewriting audioOn()
#define PWM_PIN       3
#define PWM_VALUE     OCR2B
#define LED_PIN       13
#define LED_PORT      PORTB
#define LED_BIT       5
#define PWM_INTERRUPT TIMER2_OVF_vect

// Smooth logarithmic mapping
//
uint16_t antilogTable[] = {
  64830, 64132, 63441, 62757, 62081, 61413, 60751, 60097, 59449, 58809, 58176, 57549, 56929, 56316, 55709, 55109,
  54515, 53928, 53347, 52773, 52204, 51642, 51085, 50535, 49991, 49452, 48920, 48393, 47871, 47356, 46846, 46341,
  45842, 45348, 44859, 44376, 43898, 43425, 42958, 42495, 42037, 41584, 41136, 40693, 40255, 39821, 39392, 38968,
  38548, 38133, 37722, 37316, 36914, 36516, 36123, 35734, 35349, 34968, 34591, 34219, 33850, 33486, 33125, 32768
};
uint16_t mapPhaseInc(uint16_t input) {
  return (antilogTable[input & 0x3f]) >> (input >> 6);
}

// Stepped chromatic mapping
//
uint16_t midiTable[] = {
  17, 18, 19, 20, 22, 23, 24, 26, 27, 29, 31, 32, 34, 36, 38, 41, 43, 46, 48, 51, 54, 58, 61, 65, 69, 73,
  77, 82, 86, 92, 97, 103, 109, 115, 122, 129, 137, 145, 154, 163, 173, 183, 194, 206, 218, 231,
  244, 259, 274, 291, 308, 326, 346, 366, 388, 411, 435, 461, 489, 518, 549, 581, 616, 652, 691,
  732, 776, 822, 871, 923, 978, 1036, 1097, 1163, 1232, 1305, 1383, 1465, 1552, 1644, 1742,
  1845, 1955, 2071, 2195, 2325, 2463, 2610, 2765, 2930, 3104, 3288, 3484, 3691, 3910, 4143,
  4389, 4650, 4927, 5220, 5530, 5859, 6207, 6577, 6968, 7382, 7821, 8286, 8779, 9301, 9854,
  10440, 11060, 11718, 12415, 13153, 13935, 14764, 15642, 16572, 17557, 18601, 19708, 20879,
  22121, 23436, 24830, 26306
};
uint16_t mapMidi(uint16_t input) {
  return (midiTable[(1023 - input) >> 3]);
}

// Stepped Pentatonic mapping
//
uint16_t pentatonicTable[54] = {
  0, 19, 22, 26, 29, 32, 38, 43, 51, 58, 65, 77, 86, 103, 115, 129, 154, 173, 206, 231, 259, 308, 346,
  411, 461, 518, 616, 691, 822, 923, 1036, 1232, 1383, 1644, 1845, 2071, 2463, 2765, 3288,
  3691, 4143, 4927, 5530, 6577, 7382, 8286, 9854, 11060, 13153, 14764, 16572, 19708, 22121, 26306
};

uint16_t mapPentatonic(uint16_t input) {
  uint8_t value = (1023 - input) / (1024 / 53);
  return (pentatonicTable[value]);
}


void audioOn() {
#if defined(__AVR_ATmega8__)
  // ATmega8 has different registers
  TCCR2 = _BV(WGM20) | _BV(COM21) | _BV(CS20);
  TIMSK = _BV(TOIE2);
#elif defined(__AVR_ATmega1280__)
  TCCR3A = _BV(COM3C1) | _BV(WGM30);
  TCCR3B = _BV(CS30);
  TIMSK3 = _BV(TOIE3);
#else
  // Set up PWM to 31.25kHz, phase accurate
  TCCR2A = _BV(COM2B1) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  TIMSK2 = _BV(TOIE2);
#endif
}


int tempo = 100000;
int pattern = 0;
int counter = 0;

int a1 = 0; int a2 = 0; int a3 = 0; int a4 = 0; int a5 = 0;
int b1 = 0; int b2 = 0; int b3 = 0; int b4 = 0; int b5 = 0;
int c1 = 0; int c2 = 0; int c3 = 0; int c4 = 0; int c5 = 0;
int d1 = 0; int d2 = 0; int d3 = 0; int d4 = 0; int d5 = 0;
int e1 = 0; int e2 = 0; int e3 = 0; int e4 = 0; int e5 = 0;
int f1 = 0; int f2 = 0; int f3 = 0; int f4 = 0; int f5 = 0;
int g1 = 0; int g2 = 0; int g3 = 0; int g4 = 0; int g5 = 0;
int h1 = 0; int h2 = 0; int h3 = 0; int h4 = 0; int h5 = 0;

int live_sync_phase = 0;
int live_grain1_phase = 0;
int live_grain1_decay = 0;
int live_grain2_phase = 0;
int live_grain2_decay = 0;

void setup() {

  Serial.begin(115200);

  // The three pins that select which mux pin to read or write.
  pinMode(LEDS0, OUTPUT);
  pinMode(LEDS1, OUTPUT);
  pinMode(LEDS2, OUTPUT);

  pinMode(stepInputS0, OUTPUT);
  pinMode(stepInputS1, OUTPUT);
  pinMode(stepInputS2, OUTPUT);

  pinMode(toneInputS0, OUTPUT);
  pinMode(toneInputS1, OUTPUT);
  pinMode(toneInputS2, OUTPUT);


  pinMode(PWM_PIN, OUTPUT);
  audioOn();
  pinMode(LED_PIN, OUTPUT);

  // Potentiometers and the pot mux
  pinMode(potMux, INPUT);
  pinMode(global2DecayPin, INPUT);
  pinMode(stepTempoPin, INPUT);
  pinMode(globalPhaseFreqPin, INPUT);

  // Step LEDs — just set the one LED pin to OUTPUT.
  pinMode(stepLEDMux, OUTPUT);

  // Step buttons
  pinMode(stepButtonMux, INPUT_PULLUP);
  pinMode(goButton, INPUT_PULLUP);

}

void loop() {

  counter++;

  /* Most of the time, the main loop will just advance the counter while we continue generating noise.
    Each iteration, we check the counter against our "tempo" parameter to find out if it's time yet to
    jump to the next step. */

  if (counter > tempo) {


    //Housecleaning: Just a few things to get out of the way since the counter is "full"
    counter = 0;
    if (pattern == 8) {
      pattern = 0;
    }
    pattern++;
    digitalWrite(stepLED0Pin, LOW); digitalWrite(stepLED1Pin, LOW); digitalWrite(stepLED2Pin, LOW); digitalWrite(stepLED3Pin, LOW);

    digitalWrite(stepLED4Pin, LOW); digitalWrite(stepLED5Pin, LOW); digitalWrite(stepLED6Pin, LOW); digitalWrite(stepLED7Pin, LOW);

    //Live Tweaks: Read the analog inputs associated with each "live" parameter.
    live_sync_phase = map(analogRead(globalPhaseFreqPin), 0, 1023, -500, 500);
    live_grain1_phase = map(analogRead(global1PhasePin), 0, 1023, -200, 200);
    live_grain1_decay = map(analogRead(global1DecayPin), 0, 1023, -20, 20);
    live_grain2_phase = map(analogRead(global2PhasePin), 0, 1023, -200, 200);
    live_grain2_decay = map(analogRead(global2DecayPin), 0, 1023, -50, 50);

    //Tempo Control: Read the analog inputs associated with the "tempo" parameter.
    tempo = map(analogRead(stepTempoPin), 100, 1023, 1000, 32000);
    //debug
    // Serial.print (tempo); Serial.print(" ");


    //Grab the parameters for the step that we're now in. We'll use a series of case
    //statements switched on the "pattern" variable that we incremented earlier.

    /* In each of the case routines below you'll notice that we're addressing
      each of the existing Auduino parameters and making them equal to the stored
      parameter plus the associated "live" parameter. */

    switch (pattern) {

      case 1:
        syncPhaseInc = a1 + live_sync_phase; grainPhaseInc = a2 + live_grain1_phase; grain1Decay = a3 + live_grain1_decay; grain2PhaseInc = a4 + live_grain2_phase; grain2Decay = a5 + live_grain2_decay; digitalWrite(stepLED0Pin, HIGH); break;
      case 2:
        syncPhaseInc = b1 + live_sync_phase; grainPhaseInc = b2 + live_grain1_phase; grain1Decay = b3 + live_grain1_decay; grain2PhaseInc = b4 + live_grain2_phase; grain2Decay = b5 + live_grain2_decay; digitalWrite(stepLED1Pin, HIGH); break;
      case 3:
        syncPhaseInc = c1 + live_sync_phase; grainPhaseInc = c2 + live_grain1_phase; grain1Decay = c3 + live_grain1_decay; grain2PhaseInc = c4 + live_grain2_phase; grain2Decay = c5 + live_grain2_decay; digitalWrite(stepLED2Pin, HIGH); break;
      case 4:
        syncPhaseInc = d1 + live_sync_phase; grainPhaseInc = d2 + live_grain1_phase; grain1Decay = d3 + live_grain1_decay; grain2PhaseInc = d4 + live_grain2_phase; grain2Decay = d5 + live_grain2_decay; digitalWrite(stepLED3Pin, HIGH); break;
      case 5:
        syncPhaseInc = e1 + live_sync_phase; grainPhaseInc = e2 + live_grain1_phase; grain1Decay = e3 + live_grain1_decay; grain2PhaseInc = e4 + live_grain2_phase; grain2Decay = e5 + live_grain2_decay; digitalWrite(stepLED4Pin, HIGH); break;
      case 6:
        syncPhaseInc = f1 + live_sync_phase; grainPhaseInc = f2 + live_grain1_phase; grain1Decay = f3 + live_grain1_decay; grain2PhaseInc = f4 + live_grain2_phase; grain2Decay = f5 + live_grain2_decay; digitalWrite(stepLED5Pin, HIGH); break;
      case 7:
        syncPhaseInc = g1 + live_sync_phase; grainPhaseInc = g2 + live_grain1_phase; grain1Decay = g3 + live_grain1_decay; grain2PhaseInc = g4 + live_grain2_phase; grain2Decay = g5 + live_grain2_decay; digitalWrite(stepLED6Pin, HIGH); break;
      case 8:
        syncPhaseInc = h1 + live_sync_phase; grainPhaseInc = h2 + live_grain1_phase; grain1Decay = h3 + live_grain1_decay; grain2PhaseInc = h4 + live_grain2_phase; grain2Decay = h5 + live_grain2_decay; digitalWrite(stepLED7Pin, HIGH); break;
    }

    //Check to see if the user is trying to change the step parameters.
    //This series of statements simply check for a button press from each of
    //the step buttons and call a function to change the indicated step.

    if (digitalRead(stepButton0Pin) == LOW) {
      changeStep(1);
    }
    if (digitalRead(stepButton1Pin) == LOW) {
      changeStep(2);
    }
    if (digitalRead(stepButton2Pin) == LOW) {
      changeStep(3);
    }
    if (digitalRead(stepButton3Pin) == LOW) {
      changeStep(4);
    }
    if (digitalRead(stepButton4Pin) == LOW) {
      changeStep(5);
    }
    if (digitalRead(stepButton5Pin) == LOW) {
      changeStep(6);
    }
    if (digitalRead(stepButton6Pin) == LOW) {
      changeStep(7);
    }
    if (digitalRead(stepButton7Pin) == LOW) {
      changeStep(8);
    }
  }

  //Let's check each input and see what they're up to. Uncommenting this stops the seq on step 1 and the knobs do not change value, stuck at the bottom of their range (14-16).

  //  Serial.print (grain1DecayPin); Serial.print (" ");
  //  Serial.print (grain2PhasePin); Serial.print (" ");
  //  Serial.print (grain2DecayPin); Serial.print (" ");
  //  Serial.print (syncPhasePin); Serial.print (" ");
  //  Serial.print (global1PhasePin); Serial.print (" ");
  //  Serial.print (global1DecayPin); Serial.print (" ");
  //  Serial.print (global2PhasePin); Serial.print (" ");
  //  Serial.print (global2PhasePin); Serial.print (" ");
  //  Serial.print (global2DecayPin); Serial.print (" ");
  //  Serial.print (stepTempoPin); Serial.print (" ");
  //  Serial.println (globalPhaseFreqPin); Serial.print (" ");

  //Check each step button and see what they're up to.
  //  stepButton0Pin
  //  stepButton1Pin
  //  stepButton2Pin
  //  stepButton3Pin
  //  stepButton4Pin
  //  stepButton5Pin
  //  stepButton6Pin
  //  stepButton7Pin

  //  Allocating each step LED across the third multiplexer
  //  stepLED0Pin  mux(stepLEDMux, 0)
  //  stepLED1Pin  mux(stepLEDMux, 1)
  //  stepLED2Pin  mux(stepLEDMux, 2)
  //  stepLED3Pin  mux(stepLEDMux, 3)
  //  stepLED4Pin  mux(stepLEDMux, 4)
  //  stepLED5Pin  mux(stepLEDMux, 5)
  //  stepLED6Pin  mux(stepLEDMux, 6)
  //  stepLED7Pin  mux(stepLEDMux, 7)

}

void changeStep(int step_num) {

  /* The first thing we do is to turn off all indicator lights so that we can properly indicate
    which step we're currently editing. */

  digitalWrite(stepLED0Pin, LOW); digitalWrite(stepLED1Pin, LOW); digitalWrite(stepLED2Pin, LOW); digitalWrite(stepLED3Pin, LOW);
  digitalWrite(stepLED4Pin, LOW); digitalWrite(stepLED5Pin, LOW); digitalWrite(stepLED6Pin, LOW); digitalWrite(stepLED7Pin, LOW);

  // Then indicate the appropriate step.

  switch (step_num) {

    case 1:
      digitalWrite(stepLED0Pin, HIGH); break;
    case 2:
      digitalWrite(stepLED1Pin, HIGH); break;
    case 3:
      digitalWrite(stepLED2Pin, HIGH); break;
    case 4:
      digitalWrite(stepLED3Pin, HIGH); break;
    case 5:
      digitalWrite(stepLED4Pin, HIGH); break;
    case 6:
      digitalWrite(stepLED5Pin, HIGH); break;
    case 7:
      digitalWrite(stepLED6Pin, HIGH); break;
    case 8:
      digitalWrite(stepLED7Pin, HIGH); break;
  }

  /* This next chunk of code is fairly similar to the unaltered Auduino sketch. This allows
    us to continue updating the synth parameters to the user input. That way, you can dial in
    the sound of a particular step. The while-loop traps the program flow here until the user
    pushes button 1. As the code currently stands, "live" parameters aren't applied while in
    the step editor but you could easily add the live parameters below. */

  while (1) {

    counter++;
    if (counter > tempo) {

      counter = 0;
      syncPhaseInc = mapPentatonic(analogRead(syncPhasePin));

      grainPhaseInc  = mapPhaseInc(analogRead(grain1PhasePin)) / 2;
      grain1Decay     = analogRead(grain1DecayPin) / 8;
      grain2PhaseInc = mapPhaseInc(analogRead(grain2PhasePin)) / 2;
      grain2Decay    = analogRead(grain2DecayPin) / 4;

      //Here we read the button 1 input and commit the step changes to the appropriate parameters.

      if (digitalRead(stepButton0Pin) == LOW && step_num == 1) {
        a1 = syncPhaseInc; a2 = grainPhaseInc; a3 = grain1Decay; a4 = grain2PhaseInc; a5 = grain2Decay; return;
      }
      else if (digitalRead(stepButton0Pin) == LOW && step_num == 2) {
        b1 = syncPhaseInc; b2 = grainPhaseInc; b3 = grain1Decay; b4 = grain2PhaseInc; b5 = grain2Decay; return;
      }
      else if (digitalRead(stepButton0Pin) == LOW && step_num == 3) {
        c1 = syncPhaseInc; c2 = grainPhaseInc; c3 = grain1Decay; c4 = grain2PhaseInc; c5 = grain2Decay; return;
      }
      else if (digitalRead(stepButton0Pin) == LOW && step_num == 4) {
        d1 = syncPhaseInc; d2 = grainPhaseInc; d3 = grain1Decay; d4 = grain2PhaseInc; d5 = grain2Decay; return;
      }
      else if (digitalRead(stepButton0Pin) == LOW && step_num == 5) {
        e1 = syncPhaseInc; e2 = grainPhaseInc; e3 = grain1Decay; e4 = grain2PhaseInc; e5 = grain2Decay; return;
      }
      else if (digitalRead(stepButton0Pin) == LOW && step_num == 6) {
        f1 = syncPhaseInc; f2 = grainPhaseInc; f3 = grain1Decay; f4 = grain2PhaseInc; f5 = grain2Decay; return;
      }
      else if (digitalRead(stepButton0Pin) == LOW && step_num == 7) {
        g1 = syncPhaseInc; g2 = grainPhaseInc; g3 = grain1Decay; g4 = grain2PhaseInc; g5 = grain2Decay; return;
      }
      else if (digitalRead(stepButton0Pin) == LOW && step_num == 8) {
        h1 = syncPhaseInc; h2 = grainPhaseInc; h3 = grain1Decay; h4 = grain2PhaseInc; h5 = grain2Decay; return;
      }

    }
  }
}


SIGNAL(PWM_INTERRUPT)
{
  uint8_t value;
  uint16_t output;

  syncPhaseAcc += syncPhaseInc;
  if (syncPhaseAcc < syncPhaseInc) {
    // Time to start the next grain
    grainPhaseAcc = 0;
    grain1Amp = 0x7fff;
    grain2PhaseAcc = 0;
    grain2Amp = 0x7fff;
    LED_PORT ^= 1 << LED_BIT; // Faster than using digitalWrite
  }

  // Increment the phase of the grain oscillators
  grainPhaseAcc += grainPhaseInc;
  grain2PhaseAcc += grain2PhaseInc;

  // Convert phase into a triangle wave
  value = (grainPhaseAcc >> 7) & 0xff;
  if (grainPhaseAcc & 0x8000) value = ~value;
  // Multiply by current grain amplitude to get sample
  output = value * (grain1Amp >> 8);

  // Repeat for second grain
  value = (grain2PhaseAcc >> 7) & 0xff;
  if (grain2PhaseAcc & 0x8000) value = ~value;
  output += value * (grain2Amp >> 8);

  // Make the grain amplitudes decay by a factor every sample (exponential decay)
  grain1Amp -= (grain1Amp >> 8) * grain1Decay;
  grain2Amp -= (grain2Amp >> 8) * grain2Decay;

  // Scale output to the available range, clipping if necessary
  output >>= 9;
  if (output > 255) output = 255;

  // Output to PWM (this is faster than using analogWrite)
  PWM_VALUE = output;
}


// To address a mux pin as though it's a regular pin by replacing this function call in the #define at the top

int stepMux(int whichMux, int whichMuxPin)
{
  //Choose which multiplexer pin to read by converting the address to a 3-bit number
  bit0 = bitRead(whichMuxPin, 0);
  bit1 = bitRead(whichMuxPin, 1);
  bit2 = bitRead(whichMuxPin, 2);

  // Set the Arduino pins to correspond to the bit value of the three bits
  digitalWrite(LEDS0, bit0);
  digitalWrite(LEDS1, bit1);
  digitalWrite(LEDS2, bit2);

  return (whichMux);
}

int stepInputMux(int whichMux, int whichMuxPin)
{
  bit0 = bitRead(whichMuxPin, 0);
  bit1 = bitRead(whichMuxPin, 1);
  bit2 = bitRead(whichMuxPin, 2);

  // Set the Arduino pins to correspond to the bit value of the three bits
  digitalWrite(stepInputS0, bit0);
  digitalWrite(stepInputS1, bit1);
  digitalWrite(stepInputS2, bit2);

  return (whichMux);
}

int toneInputMux(int whichMux, int whichMuxPin)
{
  bit0 = bitRead(whichMuxPin, 0);
  bit1 = bitRead(whichMuxPin, 1);
  bit2 = bitRead(whichMuxPin, 2);

  // Set the Arduino pins to correspond to the bit value of the three bits
  digitalWrite(toneInputS0, bit0);
  digitalWrite(toneInputS1, bit1);
  digitalWrite(toneInputS2, bit2);

  return (whichMux);
}



