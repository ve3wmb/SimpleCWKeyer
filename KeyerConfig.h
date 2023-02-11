#ifndef KEYERCONFIG_H
#define KEYERCONFIG_H
/*
    KeyerConfig.h - Definitions specific to SimpleMorseKeyer default options

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


#define SIDETONE_FREQ_HZ 600           // Frequency for the keyer sidetone
#define COMMAND_INPUT_TIMEOUT_MS 8000  // (8 Seconds) If command button is pressed and there is no paddle input after this amount of time, command mode is exited.
#define COMMAND_TUNE_TIMEOUT_MS 20000  // 20 second timeout on TUNE command
#define DEFAULT_SPEED_WPM 15           // 15 WPM default keyer speed
//#define KEYER_MODE_IS_IAMBIC_A       // Remove the '//' at the beginning of the line if you prefer Iambic A, otherwise the code will use Iambic B. If you don't use squeeze keying it doesn't matter. 

#endif
