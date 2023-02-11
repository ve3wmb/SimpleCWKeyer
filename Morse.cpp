////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Morse.cpp - Morse Code character encoding and decoding to/from a binary representation and playing as audio feeback via the Piezo Speaker
//
// The binary encoding scheme used here for Morse characters comes from from the QRSS/FSKCW/DFCW Beacon Keyer by Hans Summers G0UPL (copyright 2012)
// The original source code is from here : https://qrp-labs.com/images/qrssarduino/qrss.ino
//
// The remainder of this code is :
//
// Copyright (C) 2023 Michael Babineau <mbabineau.ve3wmb@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser Public License for more details.
//
// You should have received a copy of the GNU Lesser Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Morse.h"
#include "KeyerConfig.h"
#include "KeyerBoardConfig.h"

#define SPACE B11101111  // Special encoding for interword Space character

enum MORSE_SEND_STATE_TYPE {  // This is the state machine type definition for sending audible Morse feedback via the Piezo Speaker
  START,
  SEND_DIT,
  SEND_DAH,
  SEND_SPACE,
  SEND_INTER_ELEMENT,
  DONE
};


uint8_t morse_char_code(char c) {

  // This function returns the encoded Morse pattern for the ASCII character passed in.
  // Binary encoding is left-padded. Unused high-order bits are all ones.
  // The first zero is the start bit, which is discarded.
  // Processing from higher to lower order bits we skip over ones, then discard first 0 (start bit). The next bit is the first element.
  // We process each element sending a DIT or DAH, until we reach the end of the pattern.
  //
  // Pattern encoding is 0 = DIT, 1 = DAH.
  // So 'A' = B11111001, which is 1 1 1 1 1 (padding bits) 0 (start bit)  0 1 (dit, dah)
  // This excellent encoding scheme was developed by Hans, G0UPL as noted above.

  switch (c) {
    case 'A': return B11111001; break;  // A  .-
    case 'B': return B11101000; break;  // B  -...
    case 'C': return B11101010; break;  // C  -.-.
    case 'D': return B11110100; break;  // D  -..
    case 'E': return B11111100; break;  // E  .
    case 'F': return B11100010; break;  // F  ..-.
    case 'G': return B11110110; break;  // G  --.
    case 'H': return B11100000; break;  // H  ....
    case 'I': return B11111000; break;  // I  ..
    case 'J': return B11100111; break;  // J  .---
    case 'K': return B11110101; break;  // K  -.-
    case 'L': return B11100100; break;  // L  .-..
    case 'M': return B11111011; break;  // M  --
    case 'N': return B11111010; break;  // N  -.
    case 'O': return B11110111; break;  // O  ---
    case 'P': return B11100110; break;  // P  .--.
    case 'Q': return B11101101; break;  // Q  --.-
    case 'R': return B11110010; break;  // R  .-.
    case 'S': return B11110000; break;  // S  ...
    case 'T': return B11111101; break;  // T  -
    case 'U': return B11110001; break;  // U  ..-
    case 'V': return B11100001; break;  // V  ...-
    case 'W': return B11110011; break;  // W  .--
    case 'X': return B11101001; break;  // X  -..-
    case 'Y': return B11101011; break;  // Y  -.--
    case 'Z': return B11101100; break;  // Z  --..
    case '0': return B11011111; break;  // 0  -----
    case '1': return B11001111; break;  // 1  .----
    case '2': return B11000111; break;  // 2  ..---
    case '3': return B11000011; break;  // 3  ...--
    case '4': return B11000001; break;  // 4  ....-
    case '5': return B11000000; break;  // 5  .....
    case '6': return B11010000; break;  // 6  -....
    case '7': return B11011000; break;  // 7  --...
    case '8': return B11011100; break;  // 8  ---..
    case '9': return B11011110; break;  // 9  ----.
    case ' ': return SPACE; break;      // Space - equal to 4 dah lengths
    case '/': return B11010010; break;  // /  -..-.
    case '?': return B10001100; break;  // ? ..--..
    case '*': return B11001010; break;  // AR .-.-.  End of Transmission (we use * in ASCII to represent this character)
    case '#': return B00000000; break;  // ERROR .......  (we use # in ASCII to represent this)

    default: return SPACE;  // If we don't recognize the character then just return Space - equal to 4 dah lengths
  }                         // end switch
}  // end morse_char_code()




void audio_send_morse_msg(const char *msg_ptr, uint32_t dit_time_ms)
// Sends a audio Morse Response message pointed to by msg_ptr
{

  uint8_t morse_character;  // Bit pattern for the character being sent
  bool transmission_done = false;

  while (!transmission_done) {

    // call audio_send_Morse_char for each character in response message.
    if (!*msg_ptr) {
      // Terminating Null character reached
      transmission_done = true;
    } else {
      // Get the encoded bit pattern for the morse character
      morse_character = morse_char_code(*msg_ptr);
      audio_send_morse_character(morse_character, dit_time_ms);
      msg_ptr++;  // Point to the next character in the message to send

      // The last element of each character already inserts an inter-element space so we delay 2 X Dit not 3 X Dit
      delay((dit_time_ms * 2));  // Inter character spacing (equivalent to Dah length)
    }

  }  // End While

}  // end audio_send_morse_msg()


void audio_send_morse_character(uint8_t send_char, uint32_t ditDuration_ms)
// This function will send a Morse character via audio using the Piezo Speaker
{
  uint8_t character_bit;                           // Current bit number being processed in encoded character (0 to 7)
  uint32_t timeout_time;                           // Poor mans timer
  uint32_t dahDuration_ms = ditDuration_ms * 3;    // Number of ms to send a DAH at the current speed
  uint32_t spaceDuration_ms = ditDuration_ms * 4;  // Number of ms for an inter-word space at the current speed
  uint8_t morse_element;                           // The current element to be sent (DIT or DAH)

  MORSE_SEND_STATE_TYPE send_state = START;

  // Special Case for SPACE character (encoded as B11101111)
  if (send_char == SPACE) {
    timeout_time = millis() + spaceDuration_ms;
    while (millis() < timeout_time) {
      delay(5);
    }
    return;  // We are done with this character so return
  }

  // Iterate through the bits in the character to be sent, from most significant to least significant, discarding leading 1's
  // until we reach the start bit (first 0 bit).
  character_bit = 7;

  while (send_char & (1 << character_bit)) character_bit--;  // Find the start bit
  character_bit--;                                           // The next rightmost bit of the pattern, this is first character bit

  morse_element = send_char & (1 << character_bit);


  do {  // Send a character

    switch (send_state) {
      case START:
        {  // The initial state when sending a morse element

          if (morse_element == DIT) {
            send_state = SEND_DIT;
            timeout_time = millis() + ditDuration_ms;  // Set the completion time for the DIT

          } else {  // DAH
            send_state = SEND_DAH;
            timeout_time = millis() + dahDuration_ms;  // Set the completion time for the DAH
          }
          tone(PIEZO_SPKR_PIN, SIDETONE_FREQ_HZ);
          break;
        }
      case SEND_DIT:
        {
          if (millis() > timeout_time) {
            noTone(PIEZO_SPKR_PIN);                    // turn off keyer sidetone PIEZO_SPKR_Pin
            timeout_time = millis() + ditDuration_ms;  // Inter-element time same as DIT duration
            send_state = SEND_INTER_ELEMENT;
          }

          break;
        }
      case SEND_DAH:
        {  // Ok so you probably figured out that this code is identical to the case SEND_DIT, but I think this improves readability
          if (millis() > timeout_time) {
            noTone(PIEZO_SPKR_PIN);                    // turn off keyer sidetone PIEZO_SPKR_Pin
            timeout_time = millis() + ditDuration_ms;  // Inter-element time same as DIT duration
            send_state = SEND_INTER_ELEMENT;
          }
          break;
        }
      case SEND_SPACE:
        {  // This isn't needed at all as SPACE character is handled as a special case
          break;
        }
      case SEND_INTER_ELEMENT:
        {
          if (millis() > timeout_time) {
            if (character_bit == 0) {
              send_state = DONE;
            } else {
              character_bit--;  // The next rightmost bit of the pattern
              morse_element = send_char & (1 << character_bit);
              send_state = START;
            }
          }

          break;
        }
    }  // End switch (send_state)



  } while (send_state != DONE);

}  // end audio_send_morse_character()
