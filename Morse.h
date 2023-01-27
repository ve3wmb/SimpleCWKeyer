#ifndef MORSE_H
#define MORSE_H
/*
    Morse.h - Morse Code encoding and decoding definitions for Simple CW Keyer

   Copyright (C) 2023 Michael Babineau (mbabineau.ve3wmb@gmail.com)

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser Public License for more details.

   You should have received a copy of the GNU Lesser Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#define DIT 0
#define DAH 1

enum MORSE_SEND_STATE_TYPE {
  START,
  SEND_DIT,
  SEND_DAH,
  SEND_SPACE,
  SEND_INTER_ELEMENT,
  DONE
};

void audio_send_morse_character(uint8_t send_char, uint32_t ditDuration);
void audio_send_morse_msg (uint32_t dit_time_ms);
uint8_t morse_char_code(char c);

#endif
