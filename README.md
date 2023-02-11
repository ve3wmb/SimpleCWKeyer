# SimpleCWKeyer
 A Simple Arduino Morse Iambic Keyer

This project implements a compact (~4K of program space) Iambic Morse Keyer that should run on almost any Arduino board.
It supports both Iambic A and Iambic B keying, configurable at compile time. 

It supports a number of user commands that are input in Morse via the paddles after entering a Command mode by closing a momentary switch.
Commands supported are :

X - (eXchange) - This will swap the DIT and DAH paddles to make it easier for both left and right handers.
A - (Audio tone on/off) - This command will toggle the audio sidetone via a speaker or Piezo buzzer on and off.
T - (Tune) - Keys the transmitter for antenna tuner adjustment, until the command button is pushed or the command times out (nominally 20 seconds determined by COMMAND_TUNE_TIMEOUT_MS in KeyerConfig.h)
Sxx (Speed set) - Sets the keyer speed in WPM where xx ranges from 10 to 39 WPM. 
W - (Write to EEPROM) - This command will cause a write to EEPROM of the current keyer parameter state to make it persistant. Any changes to paddle orientation, sidetone, or speed via user commands will
  not be permanent unless a "W" command is issued to write these into permanent storage. On power-up the Arduino will restore settings saved to EEPROM. 

Successful Keyer Command operation is as follows : 

1) momentarily press the Command button -   The keyer responds with "K" in audible Morse Code
2) enter the command in Morse via the paddles - Keyer responds with "R", indicating the command is recognized and executed.
3) The keyer then automatically exits Command mode, indicated by an audible "AR" in Morse, and tells you that it is again ready for input by sending "K".

If the user enters Command mode via the button press but enters no command via the paddles, Command mode will time out after COMMAND_INPUT_TIMEOUT_MS (defined in KeyerCmd.h).
At this point the keyer will indicate that Command mode is terminated by sending "AR" followed by "K". 

If upon entering Command mode you input an unsupported command via the paddles, the keyer will respond with "?" in Morse and will exit Command Mode with the usual "AR" "K" sequence.

When entering additional parameters for the Speed set command (S) be sure to leave normal spacing between the individual characters so that they can be easily decoded. Invalid input will
cause the keyer to respond with error (....... i.e. 7 DITS) and it will then exit command mode with the "AR" "K" sequence, ignoring the speed change.

PIN assignments are configured in the file KeyerBoardConfig.h. You may need to change these according to whatever Arduino Board you are using. 
KeyerConfig.h contains parameters that can be changed to suit your needs :

	#define SIDETONE_FREQ_HZ 600           // Frequency for the keyer sidetone
	#define COMMAND_INPUT_TIMEOUT_MS 8000  // (8 Seconds) If command button is pressed and there is no paddle input after this amount of time, command mode is exited.
	#define COMMAND_TUNE_TIMEOUT_MS 20000  // 20 second timeout on TUNE command
	#define DEFAULT_SPEED_WPM 15           // 15 WPM default keyer speed
	//#define KEYER_MODE_IS_IAMBIC_A       // Remove the '//' at the beginning of the line if you prefer Iambic A, otherwise the code will use Iambic B. If you don't use squeeze keying it doesn't matter.

	SIDETONE_FREQ_HZ - determines the audio frequency in Hz of the sidetone sent to the Piezo Speaker.
	COMMAND_INPUT_TIMEOUT_MS - This is timout value for Command mode when there is no input. Don't make this less than 8 seconds otherwise if you set the Keyer speed to    10 WPM you will timeout before you can send a new S command to change it !
	COMMAND_TUNE_TIMEOUT_MS - Timeout value for the TUNE command.
	DEFAULT_SPEED_WPM 15 - Initial keyer speed on first boot of software install.

The file SimpleCWKeyer.png in the "Wiring" folder shows an example of how to wire the keyer up on an Arduino UNO (matches the PIN assignments in KeyerBoardConfig.h). Note that all resistors are 100 ohm and all capacitors
are 0.1 microFarads. The transmitter is keyed via a 2N2222 NPN transistor used as a switch  (green wires). The pink and yellow wires in the diagram denote the left and right paddle connections and the momentary switch 
for Command mode uses the orange wires in the diagram.It is worth mentioning that the user may decide to substitute a real speaker for the Piezo. If you wish to do so I suggest that you look at the documentation for the 
K3NG Arduino Keyer as the schematic shows how to do that. If you wish to feed the sidetone/feedback  audio into the receiver audio chain (often the case if the keyer is to be installed in a CW rig) then look at the 
documentation for the K1EL K16 Keyer boards as this will show you how to do this. There are even some recommended R/C values for a simple R/C filter to clean up the audio harmonics so that the sidetone/feedback 
sounds a bit more pleasant to the ear. 

As noted in the code files, this software is free but its use is licenced under the GNU Lessor Public License. This program is distributed via GITHUB in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

At the moment I have no plans to further enhance this code. However, if you have any questions or bug reports, I will do my best to answer at mbabineau.ve3wmb@gmail.com.  -  VE3WMB 2023





