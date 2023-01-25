// Simple CW Keyer - A simple Iambic Morse Keyer for Arduino
// January 19, 2023
// Version 0.02a 
//
// A basic iambic mode keyer that has adjustable speed
// and can be configured to operate in iambic mode a or b.
//
// Copyright (c) 2023 Michael Babineau, VE3WMB (mbabineau.ve3wmb@gmail.com)
//
//  Based heavily on the "Iambic Morse Code Keyer Sketch"
//  Original Copyright (c) 2009 Steven T. Elliott, K1EL
//
// As the code by K1EL that this sketch is based on was originally placed in the public domain under the
// GNU LESSER GENERAL PUBLIC LICENSE this new derivation maintains this licensing.
//  
// See the file called LICENSE for details.
/////////////////////////////////////////////////////////////////////////////////////////
#include "Morse.h"
//#include "Config.h"
//
// Digital Pins
//
int LEFT_PADDLE_Pin = 9;    // Left paddle input
int RIGHT_PADDLE_Pin = 10;  // Right paddle input
int PIEZO_SPKR_Pin = 11;    // Piezo Speaker
int ledPin = 13;
int cmdPin = 4;            // Command pushbutton input
int TX_SWITCH_Pin = 5;      // Radio T/R Switch - HIGH is Key down (TX)), LOW is key up (RX). 

/////////////////////////////////////////////////////////////////////////////////////////
//
//  keyerControl bit definitions
//
#define DIT_L 0x01     // Dit latch (Binary 0000 0001)
#define DAH_L 0x02     // Dah latch (Binary 0000 0010)
#define DIT_PROC 0x04  // Dit is being processed (Binary 0000 0100)
#define IAMBIC_B 0x10  // 0x00 for Iambic A, 0x01 for Iambic B (Binary 0001 0000)

#define SIDETONE_FREQ_HZ 600  // Frequency for the keyer sidetone 


////////////////////////////////////////////////////////////////////////////////////////
//
//  Keyer State Machine Defines

enum KSTYPE {
  IDLE,
  CHK_DIT,
  CHK_DAH,
  KEYED_PREP,
  KEYED,
  INTER_ELEMENT,
};

////////////////////////////////////////////////////////////////////////////////////////
//
//  Global Variables
//
uint32_t ditTime;      // Number of milliseconds per dit
uint8_t keyerControl;  // 0x1 Dit latch, 0x2 Dah latch, 0x04 Dit being processed, 0x10 0 for Iambic A or 1 for B

// We declare these so that DIT and DAH paddles can be swapped fpr both left and right users. Convention is DIT on thumb.
uint8_t DIT_PADDLE;
uint8_t DAH_PADDLE;
KSTYPE keyerState = IDLE;  // State global variable


///////////////////////////////////////////////////////////////////////////////
//
//  System Initialization
//
///////////////////////////////////////////////////////////////////////////////

void setup() {

  // Setup outputs
  pinMode(ledPin, OUTPUT);  // sets the digital pin as output
  pinMode(PIEZO_SPKR_Pin, OUTPUT);
  pinMode(TX_SWITCH_Pin, OUTPUT);

  // Setup control input pins
  pinMode(LEFT_PADDLE_Pin, INPUT_PULLUP);   // sets Left Paddle digital pin as input
  pinMode(RIGHT_PADDLE_Pin, INPUT_PULLUP);  // sets Right Paddle digital pin as input
  pinMode(cmdPin, INPUT_PULLUP);            // sets PB analog pin 3 as command switch input
  digitalWrite(ledPin, LOW);                // turn the LED off
  digitalWrite(TX_SWITCH_Pin, LOW);          // Trasmitter off

  keyerState = IDLE;
  keyerControl = IAMBIC_B;  // Make Iambic B the default mode
  loadWPM(18);              // Fix speed at 18 WPM

  // Setup for Righthanded paddle by default
  DIT_PADDLE = LEFT_PADDLE_Pin;   // Dits on right hand thumb
  DAH_PADDLE = RIGHT_PADDLE_Pin;  // Dahs on right hand index finger
}

///////////////////////////////////////////////////////////////////////////////
//
//  Main Work Loop
//
///////////////////////////////////////////////////////////////////////////////

void loop() {
  static long ktimer;
  int debounce;

  // Basic Iambic Keyer
  // keyerControl contains processing flags and keyer mode bits
  // Supports Iambic A and B
  // State machine based, uses calls to millis() for timing.

  switch (keyerState) {
    case IDLE:
      // Wait for direct or latched paddle press
      if ((digitalRead(DIT_PADDLE) == LOW) || (digitalRead(DAH_PADDLE) == LOW) || (keyerControl & 0x03)) {
        update_PaddleLatch();
        keyerState = CHK_DIT;
      }
      break;

    case CHK_DIT:
      // See if the dit paddle was pressed
      if (keyerControl & DIT_L) {
        keyerControl |= DIT_PROC;
        ktimer = ditTime;
        keyerState = KEYED_PREP;
      } else {
        keyerState = CHK_DAH;
      }
      break;

    case CHK_DAH:
      // See if dah paddle was pressed
      if (keyerControl & DAH_L) {
        ktimer = ditTime * 3;
        keyerState = KEYED_PREP;
      } else {
        keyerState = IDLE;
      }
      break;

    case KEYED_PREP:
      // Assert key down, start timing, state shared for dit or dah
      tx_key_down();
      
      ktimer += millis();                // set ktimer to interval end time
      keyerControl &= ~(DIT_L + DAH_L);  // clear both paddle latch bits
      keyerState = KEYED;                // next state
      break;

    case KEYED:
      // Wait for timer to expire
      if (millis() > ktimer) {        // are we at end of key down ?
        tx_key_up();
        
        ktimer = millis() + ditTime;  // inter-element time
        keyerState = INTER_ELEMENT;   // next state

      } else if (keyerControl & IAMBIC_B) {
        update_PaddleLatch();  // early paddle latch check in Iambic B mode
      }
      break;

    case INTER_ELEMENT:
      // Insert time between dits/dahs
      update_PaddleLatch();                     // latch paddle state
      if (millis() > ktimer) {                  // are we at end of inter-space ?
        if (keyerControl & DIT_PROC) {          // was it a dit or dah ?
          keyerControl &= ~(DIT_L + DIT_PROC);  // clear two bits
          keyerState = CHK_DAH;                 // dit done, check for dah
        } else {
          keyerControl &= ~(DAH_L);  // clear dah latch
          keyerState = IDLE;         // go idle
        }
      }
      break;
  }

  // Simple Iambic mode select
  // The mode is toggled between A & B every time switch is pressed
  // Flash LED to indicate new mode.
  if (digitalRead(cmdPin) == LOW) {
    // Give switch time to settle
    debounce = 100;
    do {
      // wait here until switch is released, we debounce to be sure
      if (digitalRead(cmdPin) == LOW) {
        debounce = 100;
      }
      delay(2);
    } while (debounce--);

    audio_send_morse_msg (ditTime);

    /*

    keyerControl ^= IAMBIC_B;       // Toggle Iambic B bit
    if (keyerControl & IAMBIC_B) {  // Flash once for A, twice for B
      flashLED(2);
    } else {
      flashLED(1);
    }
    */
  }
}

///////////////////////////////////////////////////////////////////////////////
//
//    Flash LED as a signal
//
//    count specifies the number of flashes
//
///////////////////////////////////////////////////////////////////////////////

void flashLED(int count) {
  int i;
  for (i = 0; i < count; i++) {
    digitalWrite(ledPin, HIGH);  // turn the LED on
    delay(250);
    digitalWrite(ledPin, LOW);  // turn the LED off
    delay(250);
  }
}

///////////////////////////////////////////////////////////////////////////////
//
//    Latch dit and/or dah press
//
//    Called by keyer routine
//
///////////////////////////////////////////////////////////////////////////////

void update_PaddleLatch() {
  if (digitalRead(DIT_PADDLE) == LOW) {
    keyerControl |= DIT_L;
  }
  if (digitalRead(DAH_PADDLE) == LOW) {
    keyerControl |= DAH_L;
  }
}

///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////
void tx_key_down() {
  digitalWrite(ledPin, HIGH);        // turn the LED on
  digitalWrite(TX_SWITCH_Pin, HIGH);
  tone(PIEZO_SPKR_Pin, SIDETONE_FREQ_HZ);

}

///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////
void tx_key_up() {

  digitalWrite(ledPin, LOW);    // turn the LED off
  digitalWrite(TX_SWITCH_Pin, LOW);
  noTone(PIEZO_SPKR_Pin);       // turn off keyer sidetone

}
///////////////////////////////////////////////////////////////////////////////
//
//    Calculate new time constants based on wpm value
//
///////////////////////////////////////////////////////////////////////////////

void loadWPM(int wpm) {
  ditTime = 1200 / wpm;
}
