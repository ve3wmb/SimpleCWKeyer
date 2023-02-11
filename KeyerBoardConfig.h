#ifndef KEYERBOARDCONFIG_H
#define KEYERBOARDCONFIG_H
/*
    KeyerBoardConfig.h - Definitions specific to handling keyer commands input via the paddles

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
// ** DON'T Change this value unless it differs for your Arduino model (not likely) **
#define EEADDRESS 0  // Starting address for EEPROM
//

// These PIN numbers can be changed according to whatever is available on your Arduino Model
#define LEFT_PADDLE_PIN 9    // Left paddle input pin
#define RIGHT_PADDLE_PIN 10  // Right paddle input pin
#define PIEZO_SPKR_PIN 11    // Piezo Speaker connection via 100 ohm resistor
#define LED_PIN 13
#define CMD_SWITCH_PIN 4  // Command pushbutton input
#define TX_SWITCH_PIN 5   // Radio T/R Switch - HIGH is Key down (TX)), LOW is key up (RX).


#endif
