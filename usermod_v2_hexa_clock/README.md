### Other modifications needed to the WLED tree

Add the following line to `WLED/wled00/const.h` Look for the section defining many usermods and just use the next number (in my case that was 59)
`#define USERMOD_ID_HEXA_CLOCK               59`     

Copy `platformio_override.ini` to the main folder (next to `platformio.ini`) and choose your board.
