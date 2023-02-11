#ifndef KEYERCMD_H
#define KEYERCMD_H
/*
    KeyerCmd.h - Definitions specific to handling keyer commands input via the paddles

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

// Command audio Morse Response messages
#define CMD_MODE_ENTRY_MESSAGE " K"   // Entered command mode
#define CMD_MODE_EXIT_MESSAGE " * K"  // "AR K" - Exit Command Mode
#define CMD_OK_MESSAGE " R"           // Command input via paddles is recognized
#define CMD_NOT_OK_MESSAGE " ?"       // Command input via the paddles is not supported
#define CMD_ERROR_MESSAGE " #"        // ....... Error in command parameter
#define PWR_ON_MESSAGE "OK"           // Keyer Power on Message

// Supported user commands
#define X_CMD B11101001  // X (eXchange) - Paddle swap command
#define A_CMD B11111001  // A (audio) - toggle Piezo audio sidetone on/off
#define T_CMD B11111101  // T (tune) - Key the transmitter for adjusting antenna match. Exit via command button or timeout
#define S_CMD B11110000  // S (speed-set) - set the keyer speed in WPM (Sxx where xx is in the range of 10 to 39 wpm)
#define W_CMD B11110011  // W (write to eeprom) write the current keyer settings to eeprom to make them persistant

// S command valid Parameters 
#define DIGIT_1 B11001111
#define DIGIT_2 B11000111
#define DIGIT_3 B11000011
#define DIGIT_4 B11000001
#define DIGIT_5 B11000000
#define DIGIT_6 B11010000
#define DIGIT_7 B11011000
#define DIGIT_8 B11011100
#define DIGIT_9 B11011110
#define DIGIT_0 B11011111

#endif
