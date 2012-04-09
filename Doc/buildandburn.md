If your version of the source came with commands at the head of the code, those will probably be the most accurate and up to date. 

In order to Build and Burn this code you will need two programs  
`avr-gcc` this is a compiler that the makefile calls and is required to build the hex file  
`avrdude` this is a program that burns hex files to chips

### Building AVRFID ###
  The program comes with a makefile included. This make file will compile the main.c into a usable file. In order to run the make file to compile it into a .hex file that can be uploaded to your AVR you will need to use the command in the terminal.  
      `$make hex`  
   This will generate a bunch of other files as well as a `myproject.hex` which you will use to upload the program to your chip. 

The makefile by default compiles `main.c` and gives you `myproject.hex` but these settings can be changes inside of the makefile.  
### Burning AVRFID ###
This program was written to run on an ATMEGA328 running at the internal clock frequency of 8MHz, because of this the chip does not need an external clock, only 5v and Ground. In order to tell the chip to function this way you will need to burn the fuses.  
To burn the program **with burning the fuses** use the command  
`avrdude -p m328p -c usbtiny -U flash:w:myproject.hex -U lfuse:w:0xE2:m -U hfuse:w:0xD9:m -U efuse:w:0x07:m`


After you burn the program the first time the fuses are set and you dont need to burn them again.  
To burn the program **without burning the fuses** use the command  
`avrdude -p m328p -c usbtiny -U flash:w:myproject.hex`

##### On Linux and Mac ####
  Everything should work just fine. This process was tested on Debian and Arch. You may have to run the programing method (avrdude) as root. `sudo avrdude`

##### On Windows ####
  When you use windows you have to disable digital driver signature enforcement in order to program a chip.

---
I used a variant of the usb-tiny programmer to program my chips, specifically the one from sparkfun [that can be found here](http://www.sparkfun.com/products/9825) for $15. If you used a different style of programmer you may need to change the `-c usbtiny` flag when burning to whatever style of programmer you are using


If you are still having trouble Building and Burning the code submit a bug report or email me
