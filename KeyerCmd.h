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

#define CMD_MODE_ENTRY_MESSAGE " K"
#define CMD_MODE_EXIT_MESSAGE " * K" // AR K
#define CMD_OK_MESSAGE " R"
#define CMD_NOT_OK_MESSAGE " ?"
#define CMD_ERROR_MESSAGE " #"
#define PWR_ON_MESSAGE "OK"

#define X_CMD   B11101001
#define A_CMD   B11111001
#define T_CMD   B11111101
#define S_CMD   B11110000
#define W_CMD   B11110011

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
