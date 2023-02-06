// Simple CW Keyer - A simple Iambic Morse Keyer for Arduino
// January 19, 2023
// Version 0.05a 
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
#include "KeyerConfig.h"
#include "KeyerBoardConfig.h"  // Configurations for diferent Arduino Boards
#include "Morse.h"
#include "KeyerCmd.h"

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
#define COMMAND_INPUT_TIMEOUT 8000 // 
#define COMMAND_TUNE_TIMEOUT 20000 // 20 second timeout on TUNE command 


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
  PRE_IDLE,
};

enum KEYER_CMD_MODE_TYPE {
  CMD_IDLE,
  CMD_ENTER,
  CMD_TUNE,
  CMD_SPEED_WAIT_D1,
  CMD_SPEED_WAIT_D2,
  
};

////////////////////////////////////////////////////////////////////////////////////////
//
//  Global Variables
//
uint32_t ditTime;      // Number of milliseconds per dit
uint8_t keyerControl;  // 0x1 Dit latch, 0x2 Dah latch, 0x04 Dit being processed, 0x10 0 for Iambic A or 1 for B
KSTYPE keyerState = IDLE;  // State global variable
uint8_t new_keyer_wpm; 
 


// We declare these so that DIT and DAH paddles can be swapped fpr both left and right users. Convention is DIT on thumb.
uint8_t dit_paddle;
uint8_t dah_paddle;

// We use this variable to encode Morse elements (DIT = 0, DAH =1) to encode the current character send via the paddles
// The encoding is to left pad the character with 1's. A zero start bit is to the left of the first encoded element.
// We start with B11111110 and shift in a 0 or 1 according the last element received.
uint8_t current_morse_character = B11111110;

// Keyer response messages sent as Morse Audio feedback
const char pwr_on_msg[] = PWR_ON_MESSAGE;
const char cmd_mode_entry_msg[] = CMD_MODE_ENTRY_MESSAGE;
const char cmd_mode_exit_msg[] = CMD_MODE_EXIT_MESSAGE;
const char cmd_recognized_msg[] = CMD_OK_MESSAGE;
const char cmd_not_recognized_msg[] = CMD_NOT_OK_MESSAGE;
const char cmd_error_msg[] = CMD_ERROR_MESSAGE;



// Keyer Command Mode Variables
// bool      keyer_is_in_cmd_mode = false;
uint32_t  cmd_mode_input_timeout = 0; // Maximum wait time after hitting the command button with no paddle input before auto-exit
KEYER_CMD_MODE_TYPE keyer_command_mode = CMD_IDLE;
bool sidetone_muted; 


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
  dit_paddle = LEFT_PADDLE_Pin;   // Dits on right hand thumb
  dah_paddle = RIGHT_PADDLE_Pin;  // Dahs on right hand index finger

  sidetone_muted = false;  
  audio_send_morse_msg(&pwr_on_msg[0], ditTime);

  
  // Serial.begin(9600); // for Debug
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

  // keyer_is_in_command_mode = (command_mode != CMD_IDLE);

  if (keyer_command_mode != CMD_IDLE) { // Keyer is in Command Mode
      if (millis() > cmd_mode_input_timeout){  // Check for command mode timeout (no input)

        // Special Case for TUNE MODE we need to unkey the transmitter
        if (keyer_command_mode == CMD_TUNE) {
          digitalWrite(ledPin, LOW);        // turn the LED off
          digitalWrite(TX_SWITCH_Pin, LOW); // Turn the transmitter off
          keyer_command_mode = CMD_IDLE;
      
          if (!sidetone_muted) {
          noTone(PIEZO_SPKR_Pin);
          } 
          
        }
        exit_command_mode();  
      }
  }

  switch (keyerState) {
    case IDLE:
      // Wait for direct or latched paddle press
      if ((digitalRead(dit_paddle) == LOW) || (digitalRead(dah_paddle) == LOW) || (keyerControl & 0x03)) {
        update_PaddleLatch();
        current_morse_character = B11111110; // Initialization
        keyerState = CHK_DIT;
      }
      break;

    case CHK_DIT:
      // See if the dit paddle was pressed
      if (keyerControl & DIT_L) {
        keyerControl |= DIT_PROC;
        ktimer = ditTime;
        current_morse_character = (current_morse_character << 1); // Shift a DIT (0) into the bit #0 position.
        keyerState = KEYED_PREP;
      } else {
        keyerState = CHK_DAH;
      }
      break;

    case CHK_DAH:
      // See if dah paddle was pressed
      if (keyerControl & DAH_L) {
        ktimer = ditTime * 3;
        current_morse_character = ((current_morse_character << 1) | 1 ); // Shift left one position and make bit #0 a DAH (1)
        keyerState = KEYED_PREP;
      } else {
        // keyerState = IDLE;
        ktimer = millis() + (ditTime * 2); // Character space, is ditTime x 2 because already have a trailing intercharacter space
        keyerState = PRE_IDLE;            // go idle
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
          ktimer = millis() + (ditTime * 2); // Character space, is ditTime x 2 because already have a trailing intercharacter space
          keyerState = PRE_IDLE;         // go idle
        }
      }
      break;

    case PRE_IDLE: // Wait for an intercharacter space

      // Check for direct or latched paddle press
      if ((digitalRead(dit_paddle) == LOW) || (digitalRead(dah_paddle) == LOW) || (keyerControl & 0x03)) {
        update_PaddleLatch();
        keyerState = CHK_DIT;
      } 
      else { // Check for intercharacter space
        if (millis() > ktimer) {  // We have a character 
          // Serial.println(current_morse_character);
          keyerState = IDLE;         // go idle 
          if (keyer_command_mode != CMD_IDLE) {
            process_keyer_command(current_morse_character, ditTime); 
          }      
        }
      }

      break;      
  }

  
  // Handle a command mode button press
  if (digitalRead(cmdPin) == LOW) {
    // Give the switch time to settle
    debounce = 100;
    do {
      // wait here until switch is released, we debounce to be sure
      if (digitalRead(cmdPin) == LOW) {
        debounce = 100;
      }
      delay(2);
    } while (debounce--);

    // Special case for TUNE mode
    if (keyer_command_mode == CMD_TUNE) { // Locked in TX due to TUNE Mode so exit on Command Button press
      digitalWrite(ledPin, LOW);        // turn the LED off
      digitalWrite(TX_SWITCH_Pin, LOW); // Turn the transmitter off
      keyer_command_mode = CMD_IDLE;
      
      if (!sidetone_muted) {
        noTone(PIEZO_SPKR_Pin);
      } 
      exit_command_mode();

    }
    else {

      if (keyer_command_mode == CMD_IDLE) { // Not already in command mode so enter it, otherwise ignore the button press
       keyer_command_mode = CMD_ENTER;
       audio_send_morse_msg(&cmd_mode_entry_msg[0], ditTime);
       cmd_mode_input_timeout = millis() + COMMAND_INPUT_TIMEOUT; 

      }
    }

    
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
  if (digitalRead(dit_paddle) == LOW) {
    keyerControl |= DIT_L;
  }
  if (digitalRead(dah_paddle) == LOW) {
    keyerControl |= DAH_L;
  }
}

///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////
void tx_key_down() {

  if (keyer_command_mode != CMD_IDLE) { // In Cmd mode only send the audio tone 
    digitalWrite(ledPin, HIGH);        // turn the LED on
    tone(PIEZO_SPKR_Pin, SIDETONE_FREQ_HZ); // Wedon't mute the sidetone in CMD mode
  }
  else { // Not in Command mode
    digitalWrite(ledPin, HIGH);        // turn the LED on
    digitalWrite(TX_SWITCH_Pin, HIGH); // Turn the transmitter on
    if (!sidetone_muted) {
      tone(PIEZO_SPKR_Pin, SIDETONE_FREQ_HZ);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////
void tx_key_up() {

/*
  digitalWrite(ledPin, LOW);    // turn the LED off
  digitalWrite(TX_SWITCH_Pin, LOW);
  noTone(PIEZO_SPKR_Pin);       // turn off keyer sidetone
*/
  digitalWrite(ledPin, LOW);        // turn the LED off

  if (keyer_command_mode != CMD_IDLE) { // In Cmd mode we only send the audio tone 
    noTone(PIEZO_SPKR_Pin); 
  }
  else { // Not in Command mode
    digitalWrite(TX_SWITCH_Pin, LOW); // Turn the transmitter off
    if (!sidetone_muted) {
      noTone(PIEZO_SPKR_Pin);
    }
  }

}
///////////////////////////////////////////////////////////////////////////////
//
//    Calculate new time constants based on wpm value
//
///////////////////////////////////////////////////////////////////////////////

void loadWPM(int wpm) {
  ditTime = 1200 / wpm;
}

void exit_command_mode() {
  keyer_command_mode = CMD_IDLE; 
  cmd_mode_input_timeout = 0;
  audio_send_morse_msg(&cmd_mode_exit_msg[0], ditTime); 
}

void process_keyer_command(uint8_t current_character, uint32_t ditTime_ms) {

  /*
  enum KEYER_CMD_MODE_TYPE
  CMD_IDLE,
  CMD_ENTER,
  CMD_TUNE,
  CMD_SPEED_WAIT_D1,
  CMD_SPEED_WAIT_D2,
  */

  switch (keyer_command_mode) {
 
    case CMD_ENTER : // First Command Character input

      switch (current_character) {

        case X_CMD :  // eXchange paddles
          uint8_t temp_paddle;
          temp_paddle = dit_paddle;
          dit_paddle = dah_paddle;
          dah_paddle = temp_paddle;
          audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
        break;

        case A_CMD : // Toggle keyer Audio sidetone ON/OFF
          sidetone_muted = !sidetone_muted; 
          audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();

          break;

        case T_CMD : // Enter Tune mode
          audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
          digitalWrite(ledPin, HIGH);        // turn the LED on
          digitalWrite(TX_SWITCH_Pin, HIGH); // Turn the transmitter on
          if (!sidetone_muted) tone(PIEZO_SPKR_Pin, SIDETONE_FREQ_HZ);
          keyer_command_mode = CMD_TUNE;
          cmd_mode_input_timeout = millis() + (COMMAND_TUNE_TIMEOUT);        
          break;

        case S_CMD : // Set keyer speed 
          keyer_command_mode = CMD_SPEED_WAIT_D1; // Now wait on the first digit of the speed
          new_keyer_wpm = 0; 
          break;

        case W_CMD : // Write keyer config to EEPROM .. just a placeholder for now
          audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        default :
          // unrecognized command input 
          audio_send_morse_msg (&cmd_not_recognized_msg[0], ditTime_ms);
          exit_command_mode();          

      } // End switch on current character
      break;


      case CMD_SPEED_WAIT_D1 :  // Waiting on first digit of new speed (Speed range supported is 10 to 39 wpm) 

        switch (current_character) {
          case DIGIT_1 :
            new_keyer_wpm = 10;
            keyer_command_mode = CMD_SPEED_WAIT_D2;
            break;

          case DIGIT_2 :
            new_keyer_wpm = 20;
            keyer_command_mode = CMD_SPEED_WAIT_D2;
            break;

          case DIGIT_3 :
            new_keyer_wpm = 20;
            keyer_command_mode = CMD_SPEED_WAIT_D2;
            break;

          default : // Invalid speed input 
            new_keyer_wpm = 20;
            audio_send_morse_msg (&cmd_error_msg[0], ditTime_ms);
            exit_command_mode();
            break;

        }
        break;

    case CMD_SPEED_WAIT_D2 :  // Waiting on second digit of new speed (Speed range supported is 10 to 39 wpm)
      
        switch (current_character) {

          case DIGIT_1 :
            new_keyer_wpm = new_keyer_wpm + 1;
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break;

          case DIGIT_2 :
            new_keyer_wpm = new_keyer_wpm + 2;
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break;

          case DIGIT_3 :
            new_keyer_wpm = new_keyer_wpm + 3;
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break;

          case DIGIT_4 :
            new_keyer_wpm = new_keyer_wpm + 4;
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break;

          case DIGIT_5 :
            new_keyer_wpm = new_keyer_wpm + 5;
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break;            

          case DIGIT_6 :
            new_keyer_wpm = new_keyer_wpm + 6;
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break; 

          case DIGIT_7 :
            new_keyer_wpm = new_keyer_wpm + 7;
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break; 

          case DIGIT_8 :
            new_keyer_wpm = new_keyer_wpm + 8;
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break; 

          case DIGIT_9 :
            new_keyer_wpm = new_keyer_wpm + 9;
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break; 

          case DIGIT_0 :
            loadWPM(new_keyer_wpm);
            audio_send_morse_msg (&cmd_recognized_msg[0], ditTime_ms);
            exit_command_mode();
            break;                                            

          default : // Invalid speed input for 2nd digit
            audio_send_morse_msg (&cmd_error_msg[0], ditTime_ms);
            exit_command_mode();
            break;

        }
        break;

    case CMD_IDLE : break; // This is not expected 

  } // End Switch on Keyer Command Mode

  
} // End process_keyer_command()

