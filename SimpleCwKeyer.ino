// Simple CW Keyer - A simple Iambic Morse Keyer for Arduino
// January 19, 2023
// Version 0.07a
//
// A basic Iambic Mode keyer that has adjustable speed
// and can be pre-configured to operate in iambic mode A or B at compile time.
// SEE README.md for details on operation and features.
//
// Copyright (c) 2023 Michael Babineau, VE3WMB (mbabineau.ve3wmb@gmail.com)
//
//  Based heavily on the "Iambic Morse Code Keyer Sketch"
//  Original Copyright (c) 2009 Steven T. Elliott, K1EL
//
// As the code by K1EL that this sketch is based on was originally placed in the public domain under the
// GNU LESSER GENERAL PUBLIC LICENSE this new derivation maintains this licensing.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser Public License (source file called LICENSE) for more details .
//
//  If not present, see <http://www.gnu.org/licenses/>.
//
/////////////////////////////////////////////////////////////////////////////////////////

#include "KeyerConfig.h"       // Definitions for parameters specific to keyer operation (mostly user configurable)
#include "KeyerBoardConfig.h"  // Configurations for different Arduino Boards (user configurable)
#include "Morse.h"             // Definitions for binary encoding/decoding of Morse Characters
#include "KeyerCmd.h"          // Keyer Command Definitions
#include <EEPROM.h>


/////////////////////////////////////////////////////////////////////////////////////////
//
//  g_keyerControl bit definitions
//
////////////////////////////////////////////////////////////////////////////////////////
#define DIT_L 0x01     // Dit latch (Binary 0000 0001)
#define DAH_L 0x02     // Dah latch (Binary 0000 0010)
#define DIT_PROC 0x04  // Dit is being processed (Binary 0000 0100)
#define IAMBIC_B 0x10  // 0x00 for Iambic A, 0x10 for Iambic B (Binary 0001 0000)
#define IAMBIC_A 0

////////////////////////////////////////////////////////////////////////////////////////
//
//  Morse Iambic Keyer State Machine type
//
///////////////////////////////////////////////////////////////////////////////////////
// Keyer State machine type - states for sending of Morse elements based on paddle input
enum KSTYPE {
  IDLE,
  CHK_DIT,
  CHK_DAH,
  KEYED_PREP,
  KEYED,
  INTER_ELEMENT,
  PRE_IDLE,
};


// Keyer Command State type - states for processing of Commands entered via the paddles after entering Command Mode
enum KEYER_CMD_STATE_TYPE {
  CMD_IDLE,
  CMD_ENTER,
  CMD_TUNE,
  CMD_SPEED_WAIT_D1,
  CMD_SPEED_WAIT_D2,

};

// Data type for storing Keyer settings in EEPROM
struct KeyerConfigType {
  uint32_t ms_per_dit;         // Speed
  uint8_t dit_paddle_pin;      // Dit paddle pin number
  uint8_t dah_paddle_pin;      // Dah Paddle pin number
  uint8_t iambic_keying_mode;  // 0 - Iambic-A, 1 Iambic-B
  uint8_t sidetone_is_muted;   //
  uint32_t num_writes;         // Number of writes to EEPROM, incremented on each 'W' command
  uint32_t data_checksum;      // Checksum containing a 32-bit sum of all of the other fields
};


////////////////////////////////////////////////////////////////////////////////////////
//
//  Global Variables
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// I do recognized that this is quite a lot of global variables for such a small program
// and many would frown upon this. In general it is not great design practice but it was
// easiest to start out this way when prototyping the code and at the moment I can't be bothered
// to restructure the code to implement proper data-hiding since it is working quite well and I don't want
// to break it. For now I have prefixed the globals with g_ as a reminder to be careful of what code is changing these.
// Perhaps in a future iteration of code I will come back to address this.  - VE3WMB
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t g_ditTime;           // Number of milliseconds per dit
uint8_t g_keyerControl;       // 0x1 Dit latch, 0x2 Dah latch, 0x04 Dit being processed, 0x10 for Iambic A or 1 for B
KSTYPE g_keyerState = IDLE;   // Keyer state global variable
KeyerConfigType g_ks_eeprom;  // Persistant Keyer config info - written to EEPROM on 'W' command.
uint8_t g_new_keyer_wpm;
bool g_sidetone_muted;  // If 'true' the Piezo speaker output is muted except for when in command mode


// We declare these as variables so that DIT and DAH paddles can be swapped for both left and right hand users. Convention is DIT on thumb.
uint8_t g_dit_paddle;  // Current PIN number for Dit paddle
uint8_t g_dah_paddle;  // Current PIN number for Dah paddle

// We use this variable to encode Morse elements (DIT = 0, DAH =1) to capture the current character sent via the paddles which is needed for user commands.
// The encoding practice is to left pad the character with 1's. A zero start bit is to the left of the first encoded element.
// We start with B11111110 and shift left with a 0 or 1 from the least significant bit, according the last Morse element received (DIT = 0, DAH =1).
uint8_t g_current_morse_character = B11111110;

// Keyer Command Mode Variables
uint32_t g_cmd_mode_input_timeout = 0;                  // Maximum wait time after hitting the command button with no paddle input before Command Mode auto-exit
KEYER_CMD_STATE_TYPE g_keyer_command_state = CMD_IDLE;  // This is the state variable for the Command Mode state machine


// Keyer response messages sent as Morse Audio feedback via the Piezo Speaker (message contents defined in KeyerCmd.h)
// Note that these are also globals but given that they are constants there is no chance that they can be accidentally overwritten.
const char pwr_on_msg[] = PWR_ON_MESSAGE;
const char cmd_mode_entry_msg[] = CMD_MODE_ENTRY_MESSAGE;
const char cmd_mode_exit_msg[] = CMD_MODE_EXIT_MESSAGE;
const char cmd_recognized_msg[] = CMD_OK_MESSAGE;
const char cmd_not_recognized_msg[] = CMD_NOT_OK_MESSAGE;
const char cmd_error_msg[] = CMD_ERROR_MESSAGE;


///////////////////////////////////////////////////////////////////////////////
//
//  System Initialization - this runs on power up
//
///////////////////////////////////////////////////////////////////////////////

void setup() {

  // Setup output pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIEZO_SPKR_PIN, OUTPUT);
  pinMode(TX_SWITCH_PIN, OUTPUT);

  // Setup input pins
  pinMode(LEFT_PADDLE_PIN, INPUT_PULLUP);   // sets Left Paddle digital pin as input
  pinMode(RIGHT_PADDLE_PIN, INPUT_PULLUP);  // sets Right Paddle digital pin as input
  pinMode(CMD_SWITCH_PIN, INPUT_PULLUP);    // sets pin as command mode switch input

  digitalWrite(LED_PIN, LOW);        // turn the LED off
  digitalWrite(TX_SWITCH_PIN, LOW);  // Transmitter off

  g_keyerState = IDLE;

  EEPROM.get(EEADDRESS, g_ks_eeprom);  // Read the stored config data from EEPROM
  if (!validate_ee_checksum()) {       // does EEPROM checksum equal the calculated checksum

    // Checksum didn't match stored data so assume that this is the first time this code has run

    g_keyerControl = IAMBIC_B;   // Make Iambic B the default mode

    #if defined (KEYER_MODE_IS_IAMBIC_A)
      g_keyerControl = IAMBIC_A; // Override with Iambic A via conditional compilation
    #endif
    loadWPM(DEFAULT_SPEED_WPM);  // Set initial keyer speed to default

    // Setup for Righthanded paddle by default
    g_dit_paddle = LEFT_PADDLE_PIN;   // Dits on right hand thumb
    g_dah_paddle = RIGHT_PADDLE_PIN;  // Dahs on right hand index finger

    g_sidetone_muted = true;  // Default to no sidetone

    // Store these defaults and write settings to EEPROM
    g_ks_eeprom.ms_per_dit = g_ditTime;
    g_ks_eeprom.dit_paddle_pin = g_dit_paddle;
    g_ks_eeprom.dah_paddle_pin = g_dah_paddle;
    g_ks_eeprom.iambic_keying_mode = g_keyerControl;
    g_ks_eeprom.sidetone_is_muted = true;
    g_ks_eeprom.num_writes = 1;
    g_ks_eeprom.data_checksum = calculate_ee_checksum();
    EEPROM.put(EEADDRESS, g_ks_eeprom);
  } else {  // Checksum matched so restore previous settings that were saved in EEPROM
    g_ditTime = g_ks_eeprom.ms_per_dit;
    g_dit_paddle = g_ks_eeprom.dit_paddle_pin;
    g_dah_paddle = g_ks_eeprom.dah_paddle_pin;
    g_keyerControl = g_ks_eeprom.iambic_keying_mode;
    g_sidetone_muted = (g_ks_eeprom.sidetone_is_muted != 0);
  }

  audio_send_morse_msg(&pwr_on_msg[0], g_ditTime);  // Send the OK message via the Piezo Speaker

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

  // Handle Command Mode timer and sprcial case of Command TUNE
  if (g_keyer_command_state != CMD_IDLE) {      // Keyer is in Command Mode (i.e. command button was depressed)
    if (millis() > g_cmd_mode_input_timeout) {  // Check for command mode timer expiry (no input).

      // Special Case if we are in TUNE MODE - we need to unkey the transmitter if the timer expired
      if (g_keyer_command_state == CMD_TUNE) {
        digitalWrite(LED_PIN, LOW);        // turn the LED off
        digitalWrite(TX_SWITCH_PIN, LOW);  // Turn the transmitter off
        g_keyer_command_state = CMD_IDLE;

        if (!g_sidetone_muted) {
          noTone(PIEZO_SPKR_PIN);
        }
      }
      exit_command_mode();
    }
  }

  // This state machine translates paddle input into DITS and DAHs and keys the transmitter.
  switch (g_keyerState) {
    case IDLE:
      // Wait for direct or latched paddle press
      if ((digitalRead(g_dit_paddle) == LOW) || (digitalRead(g_dah_paddle) == LOW) || (g_keyerControl & 0x03)) {
        update_PaddleLatch();
        g_current_morse_character = B11111110;  // Sarting point for accumlating a character input via paddles
        g_keyerState = CHK_DIT;
      }
      break;

    case CHK_DIT:
      // See if the dit paddle was pressed
      if (g_keyerControl & DIT_L) {
        g_keyerControl |= DIT_PROC;
        ktimer = g_ditTime;
        g_current_morse_character = (g_current_morse_character << 1);  // Shift a DIT (0) into the bit #0 position.
        g_keyerState = KEYED_PREP;
      } else {
        g_keyerState = CHK_DAH;
      }
      break;

    case CHK_DAH:
      // See if dah paddle was pressed
      if (g_keyerControl & DAH_L) {
        ktimer = g_ditTime * 3;
        g_current_morse_character = ((g_current_morse_character << 1) | 1);  // Shift left one position and make bit #0 a DAH (1)
        g_keyerState = KEYED_PREP;
      } else {
        ktimer = millis() + (g_ditTime * 2);  // Character space, is g_ditTime x 2 because already have a trailing intercharacter space
        g_keyerState = PRE_IDLE;
      }
      break;

    case KEYED_PREP:
      // Assert key down, start timing, state shared for dit or dah
      tx_key_down();

      ktimer += millis();                  // set ktimer to interval end time
      g_keyerControl &= ~(DIT_L + DAH_L);  // clear both paddle latch bits
      g_keyerState = KEYED;                // next state
      break;

    case KEYED:
      // Wait for timer to expire
      if (millis() > ktimer) {  // are we at end of key down ?
        tx_key_up();

        ktimer = millis() + g_ditTime;  // inter-element time
        g_keyerState = INTER_ELEMENT;   // next state

      } else if (g_keyerControl & IAMBIC_B) {
        update_PaddleLatch();  // early paddle latch check in Iambic B mode
      }
      break;

    case INTER_ELEMENT:
      // Insert time between dits/dahs
      update_PaddleLatch();                       // latch paddle state
      if (millis() > ktimer) {                    // are we at end of inter-space ?
        if (g_keyerControl & DIT_PROC) {          // was it a dit or dah ?
          g_keyerControl &= ~(DIT_L + DIT_PROC);  // clear two bits
          g_keyerState = CHK_DAH;                 // dit done, check for dah
        } else {
          g_keyerControl &= ~(DAH_L);           // clear dah latch
          ktimer = millis() + (g_ditTime * 2);  // Character space, is g_ditTime x 2 because already have a trailing intercharacter space
          g_keyerState = PRE_IDLE;              // go idle
        }
      }
      break;

    case PRE_IDLE:  // Wait for an intercharacter space

      // Check for direct or latched paddle press
      if ((digitalRead(g_dit_paddle) == LOW) || (digitalRead(g_dah_paddle) == LOW) || (g_keyerControl & 0x03)) {
        update_PaddleLatch();
        g_keyerState = CHK_DIT;
      } else {                    // Check for intercharacter space
        if (millis() > ktimer) {  // We have a character
          // Serial.println(g_current_morse_character);
          g_keyerState = IDLE;  // go idle
          if (g_keyer_command_state != CMD_IDLE) {
            process_keyer_command(g_current_morse_character, g_ditTime);
          }
        }
      }

      break;
  }


  // Handle a command mode button press
  if (digitalRead(CMD_SWITCH_PIN) == LOW) {
    // Give the switch time to settle
    debounce = 100;
    do {
      // wait here until switch is released, we debounce to be sure
      if (digitalRead(CMD_SWITCH_PIN) == LOW) {
        debounce = 100;
      }
      delay(2);
    } while (debounce--);

    // Special case for TUNE mode
    if (g_keyer_command_state == CMD_TUNE) {  // We are already locked in TX due to TUNE Mode so exit TUNE on a Command Button press
      digitalWrite(LED_PIN, LOW);             // turn the LED off
      digitalWrite(TX_SWITCH_PIN, LOW);       // Turn the transmitter off
      g_keyer_command_state = CMD_IDLE;

      if (!g_sidetone_muted) {
        noTone(PIEZO_SPKR_PIN);
      }
      exit_command_mode();

    } else {  // Not TUNE MODE

      if (g_keyer_command_state == CMD_IDLE) {  // Not already in command mode so enter it, otherwise ignore a 2nd button press
        g_keyer_command_state = CMD_ENTER;
        audio_send_morse_msg(&cmd_mode_entry_msg[0], g_ditTime);
        g_cmd_mode_input_timeout = millis() + COMMAND_INPUT_TIMEOUT_MS;
      }
    }
  }
} // end of Loop 


///////////////////////////////////////////////////////////////////////////////
//
//    Latch dit and/or dah press
//
///////////////////////////////////////////////////////////////////////////////

void update_PaddleLatch() {
  if (digitalRead(g_dit_paddle) == LOW) {
    g_keyerControl |= DIT_L;
  }
  if (digitalRead(g_dah_paddle) == LOW) {
    g_keyerControl |= DAH_L;
  }
}

///////////////////////////////////////////////////////////////////////////////
//       Key the Transmitter
///////////////////////////////////////////////////////////////////////////////
void tx_key_down() {

  if (g_keyer_command_state != CMD_IDLE) {   // In Cmd mode only send the audio tone
    digitalWrite(LED_PIN, HIGH);             // turn the LED on
    tone(PIEZO_SPKR_PIN, SIDETONE_FREQ_HZ);  // We don't mute the sidetone in CMD mode
  } else {                                   // Not in Command mode
    digitalWrite(LED_PIN, HIGH);             // turn the LED on
    digitalWrite(TX_SWITCH_PIN, HIGH);       // Turn the transmitter on
    if (!g_sidetone_muted) {
      tone(PIEZO_SPKR_PIN, SIDETONE_FREQ_HZ); // Sidetone on if not muted
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
//    Un-key the transmitter
///////////////////////////////////////////////////////////////////////////////
void tx_key_up() {

  digitalWrite(LED_PIN, LOW);  // turn the LED off

  if (g_keyer_command_state != CMD_IDLE) {  // In Cmd mode sowe only send the audio tone
    noTone(PIEZO_SPKR_PIN);
  } else {                             // Not in Command mode
    digitalWrite(TX_SWITCH_PIN, LOW);  // Turn the transmitter off
    if (!g_sidetone_muted) {
      noTone(PIEZO_SPKR_PIN);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
//
//    Calculate new Dit duration in ms based on wpm value
//
///////////////////////////////////////////////////////////////////////////////

void loadWPM(int wpm) {
  g_ditTime = 1200 / wpm;
}




void exit_command_mode() {
  g_keyer_command_state = CMD_IDLE;
  g_cmd_mode_input_timeout = 0;
  audio_send_morse_msg(&cmd_mode_exit_msg[0], g_ditTime);
}

/////////////////////////////////////////////////////////////////////////////
// Use the decoded send character to determine the current user-command
// and execute it. 
////////////////////////////////////////////////////////////////////////////
void process_keyer_command(uint8_t current_character, uint32_t ditTime_ms) {


  switch (g_keyer_command_state) {

    case CMD_ENTER:  // First Command Character input

      switch (current_character) {

        case X_CMD:  // eXchange paddles
          uint8_t temp_paddle;
          temp_paddle = g_dit_paddle;
          g_dit_paddle = g_dah_paddle;
          g_dah_paddle = temp_paddle;
          g_ks_eeprom.dah_paddle_pin = g_dah_paddle;
          g_ks_eeprom.dit_paddle_pin = g_dit_paddle;
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case A_CMD:  // Toggle keyer Audio sidetone ON/OFF
          g_sidetone_muted = !g_sidetone_muted;
          g_ks_eeprom.sidetone_is_muted = g_sidetone_muted;
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case T_CMD:  // Enter Tune mode, exit via timeout or Cmd button press
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          digitalWrite(LED_PIN, HIGH);        // turn the LED on
          digitalWrite(TX_SWITCH_PIN, HIGH);  // Turn the transmitter on
          if (!g_sidetone_muted) tone(PIEZO_SPKR_PIN, SIDETONE_FREQ_HZ);
          g_keyer_command_state = CMD_TUNE;
          g_cmd_mode_input_timeout = millis() + (COMMAND_TUNE_TIMEOUT_MS);
          break;

        case S_CMD:                                   // Set keyer speed in WPM
          g_keyer_command_state = CMD_SPEED_WAIT_D1;  // Now wait on the first digit of the speed
          g_new_keyer_wpm = 0;
          break;

        case W_CMD:  // Write keyer config to EEPROM
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          g_ks_eeprom.num_writes++;                             // Increment the number of writes
          g_ks_eeprom.data_checksum = calculate_ee_checksum();  // Recalculate and save the checksum
          EEPROM.put(EEADDRESS, g_ks_eeprom);                   // Write Keyer State Variables to EEPROM
          exit_command_mode();
          break;

        default:
          // unrecognized command input os ignore it and send an appropriate audio response
          audio_send_morse_msg(&cmd_not_recognized_msg[0], ditTime_ms);
          exit_command_mode();

      }  // End switch on current character
      break;


    case CMD_SPEED_WAIT_D1:  // Waiting on first digit of new speed (Speed range supported is 10 to 39 wpm)

      switch (current_character) {
        case DIGIT_1:
          g_new_keyer_wpm = 10;
          g_keyer_command_state = CMD_SPEED_WAIT_D2;
          break;

        case DIGIT_2:
          g_new_keyer_wpm = 20;
          g_keyer_command_state = CMD_SPEED_WAIT_D2;
          break;

        case DIGIT_3:
          g_new_keyer_wpm = 30;
          g_keyer_command_state = CMD_SPEED_WAIT_D2;
          break;

        default:  // Invalid speed input
          g_new_keyer_wpm = 0;
          audio_send_morse_msg(&cmd_error_msg[0], ditTime_ms);
          exit_command_mode();
          break;
      }
      break;

    case CMD_SPEED_WAIT_D2:  // Waiting on second digit of new speed (Speed range supported is 10 to 39 wpm)

      switch (current_character) { // This is a bit repetitive but it should be clear what is happening

        case DIGIT_1:
          g_new_keyer_wpm = g_new_keyer_wpm + 1;
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case DIGIT_2:
          g_new_keyer_wpm = g_new_keyer_wpm + 2;
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case DIGIT_3:
          g_new_keyer_wpm = g_new_keyer_wpm + 3;
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case DIGIT_4:
          g_new_keyer_wpm = g_new_keyer_wpm + 4;
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case DIGIT_5:
          g_new_keyer_wpm = g_new_keyer_wpm + 5;
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case DIGIT_6:
          g_new_keyer_wpm = g_new_keyer_wpm + 6;
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case DIGIT_7:
          g_new_keyer_wpm = g_new_keyer_wpm + 7;
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case DIGIT_8:
          g_new_keyer_wpm = g_new_keyer_wpm + 8;
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case DIGIT_9:
          g_new_keyer_wpm = g_new_keyer_wpm + 9;
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        case DIGIT_0:
          loadWPM(g_new_keyer_wpm);
          g_ks_eeprom.ms_per_dit = g_ditTime;  // In case a W commands follows.
          audio_send_morse_msg(&cmd_recognized_msg[0], ditTime_ms);
          exit_command_mode();
          break;

        default:  // Invalid speed input for 2nd digit so don't update the keyer speed
          audio_send_morse_msg(&cmd_error_msg[0], ditTime_ms);
          exit_command_mode();
          break;
      }
      break;

    case CMD_IDLE: break;  // This is not expected as we set the commands state to CMD_ENTER when the button is pressed

  }  // End Switch on Keyer Command Mode

}  // End process_keyer_command()


// Calculate the 32-bit checksum value for the state data stored in EEPROM
// The only field not used in the checksum calculation is the stored checksum which should be obvious
uint32_t calculate_ee_checksum() {
  uint32_t calc_checksum = 0;
  calc_checksum = g_ks_eeprom.ms_per_dit + g_ks_eeprom.dit_paddle_pin + g_ks_eeprom.dah_paddle_pin + g_ks_eeprom.iambic_keying_mode + g_ks_eeprom.sidetone_is_muted + g_ks_eeprom.num_writes;
  return (calc_checksum);
}

// Verify that the stored EEPROM checksum matches the calculated value
bool validate_ee_checksum() {
  return (g_ks_eeprom.data_checksum == (calculate_ee_checksum()));
}
