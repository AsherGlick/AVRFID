 /****************************************************************************\
/         This program was written by Asher Glick aglick@tetrakai.com          \
\             This program is currently under the GNU GPL licence              /
 \****************************************************************************/

/****************** CHIP SETTINGS ******************\
| This program was designed to run on an ATMEGA328  |
| chip running without an external clock at 8MHz    |
\***************************************************/

/********** FUSE SETTINGS **********\
|   Low Fuse 0xE2                   |
|  High Fuse 0xD9                   |       +- AVRDUDE COMMANDS -+
| Extra Fuse 0x07                   |       | -U lfuse:w:0xe0:m  |
|                                   |       | -U hfuse:w:0xd9:m  |
| These fuse calculations are       |       | -U efuse:w:0xff:m  |
| based off of the usbtiny AVR      |       +--------------------+
| programmer. Other programmers     |
| may have a different fuse number  |
\***********************************/

/************************** AVRDUDE command for 8MHz **************************\ 
| sudo avrdude -p m328p -c usbtiny -U flash:w:myproject.hex                    |
|                       -U lfuse:w:0xE2:m -U hfuse:w:0xD9:m -U efuse:w:0x07:m  |
|                                                                              |
| NOTE: when messing with fuses, do this at your own risk. These fuses for the |
|        ATMEGA328P (ATMEGA328) worked for me, however if they do not work for |
|        you, it is not my fault                                               |
| NOTE: '-c usbtiny' may be incorrect if you are using a different programmer  |
\******************************************************************************/

// Custom values
// These values may need to be changed depending on the servo that you are using
#define SERVO_OPEN 575    // open signal value for the servo
#define SERVO_CLOSE 1000  // close signal value for the servo

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define ARRAYSIZE 900   // Raw RFID Data Buffer Size

int * begin;            // points to the bigining of the array
int * names;            // array of valid ID numbers
int namesize;           // size of array of valid ID numbers
volatile int iter;      // the iterator for the placement of count in the array
volatile int count;     // counts 125kHz pulses
volatile int lastpulse; // last value of DEMOD_OUT
volatile int on;        // stores the value of DEMOD_OUT in the interrupt

/********************************* ADD NAMES *********************************\
| This function add allocates the ammount of memory that will be needed to    |
| store the list of names, and adds all the saved names to the allocated      |
| memory for use later in the program                                         |
\*****************************************************************************/
void addNames(void) {
  namesize = 2; // number of IDs in the access list
  names = malloc (sizeof(int) * namesize);
  // change or add more IDs after this point
  names [0] = 12345;
  names [1] = 67890;
}

/******************************* INT0 INTERRUPT *******************************\
| This ISR(INT0_vect) is the interrupt function for INT0. This function is the |
| function that is run each time the 125kHz pulse goes HIGH.                   |
| 1) If this pulse is in a new wave then put the count of the last wave into   |
|     the array                                                                |
| 2) Add one to the count (count stores the number of 125kHz pulses in each    |
|     wave                                                                     |
\******************************************************************************/
ISR(INT0_vect) {
  //Save the value of DEMOD_OUT to prevent re-reading on the same group
  on =(PINB & 0x01); 
  // if wave is rising (end of the last wave)
  if (on == 1 && lastpulse == 0 ) {
    // write the data to the array and reset the cound
    begin[iter] = count;
    count = 0;
    iter = iter + 1;
  }
  count = count + 1;
  lastpulse = on;
}

/************************************ WAIT ************************************\
| A generic wait function                                                      |
\******************************************************************************/
void wait (unsigned long time) {
  long i;
  for (i = 0; i < time; i++) {
    asm volatile ("nop");
  }
}

/******************************* CONVERT INPUT *******************************\
| This function converts the 45 bit input (ints representing bools) into the  |
| decimal number represented on the card. It strips off the first 28 bits     |
| and the last bit (the parady bit) and returns a two byte number generated   |
| with the remaining 16 bits                                                  |
\*****************************************************************************/
int convertInput (int array[45]) {
  int result = 0;
  if (array[28]) result += 32768;
  if (array[29]) result += 16348;
  if (array[30]) result += 8192;
  if (array[31]) result += 4096;
  if (array[32]) result += 2048;
  if (array[33]) result += 1024;
  if (array[34]) result += 512;
  if (array[35]) result += 256;
  if (array[36]) result += 128;
  if (array[37]) result += 64;
  if (array[38]) result += 32;
  if (array[39]) result += 16;
  if (array[40]) result += 8;
  if (array[41]) result += 4;
  if (array[42]) result += 2;
  if (array[43]) result += 1;
  return result;
}

/********************************* Search Tag *********************************\
| This function searches for a tag in the list of tags stored in the flash     |
| memory, if the tag is found then the function returns 1 (true) if the tag    |
| is not found then the function returns 0 (false)                             |
\******************************************************************************/
int searchTag (int tag) {
  int i;
  for (i = 0; i < namesize; i++) {
    if (tag == names[i]) {
      return 1;
    }
  }
  return 0;
}

/******************************* Analize Input *******************************\
| analizeInput(void) parses through the global variable and gets the 45 bit   |
| id tag.                                                                     |
| 1) Converts raw pulse per wave count (5,6,7) to binary data (0,1)           |
| 2) Finds a start tag in the code                                            |
| 3) Parses the data from multibit code (11111000000000000111111111100000) to |
|     singlebit manchester code (100110) untill it finds an end tag           |
| 4) Converts manchester code (100110) to binary code (010)                   |
\*****************************************************************************/
void analizeInput (void) {
  int i;                // Generic for loop 'i' counter
  int inARow = 0;       // number of identical bits in a row
  int lastVal = 0;      // value of the identical bits in a row
  int resultArray[90];  // Parsed Bit code in manchester
  int resultArray_index = 0;
  int finalArray[45];   //Parsed Bit Code out of manchester
  int finalArray_index = 0;
  
  // Initilize the arrays so that any errors or unchanged values show up as 2s
  for (i = 0; i < 90; i ++) {
    resultArray[i] = 2;
  }
  for (i = 0; i < 45; i++) {
    finalArray[i] = 2;
  }
  
  //------------------------------------------
  // Convert raw data to binary
  //------------------------------------------
  for (i = 1; i < ARRAYSIZE; i++) {
    if (begin[i] == 5) {
      begin[i] = 0;
    }
    else if (begin[i] == 7) {
      begin[i] = 1;
    }
    else if (begin[i] == 6) {
       begin[i] = begin[i-1];
    }
    else {
      begin[i] = -2;
    }
  }
    
  //------------------------------------------
  // Find Start Label
  //------------------------------------------
  for (i = 0; i < ARRAYSIZE; i++) {
    if (begin [i] == lastVal) {
      inARow++;
    }
    else {
      // End of the group of bits with the same value
      if (inARow >= 15 && lastVal == 1) {
        // Start tag found
        break;
      }
      // group of bits was not a start tag, search next tag
      inARow = 1;
      lastVal = begin[i];
    }
  }
  int start = i;
  PORTB |= 0x10;
  
  //------------------------------------------
  // PARSE TO BIT DATA
  //------------------------------------------
  for (;i < ARRAYSIZE; i++) {
    if (begin [i] == lastVal) {
      inARow++;
    }
    else {
      // End of the group of bits with the same value
      if (inARow >= 4 && inARow <= 8) {
        // there are between 4 and 8 bits of the same value in a row
        // Add one bit to the resulting array
        resultArray[resultArray_index] = lastVal;
        resultArray_index += 1;
      }
      else if (inARow >= 9 && inARow <= 14) {
        // there are between 9 and 14 bits of the same value in a row
        // Add two bits to the resulting array
        resultArray[resultArray_index] = lastVal;
        resultArray[resultArray_index+1] = lastVal;
        resultArray_index += 2;
      }
      else if (inARow >= 15 && lastVal == 0) {
        // there are more then 15 identical bits in a row, and they are 0s
        // this is an end tag
        break;
      }
      // group of bits was not the end tag, continue parsing data
      inARow = 1;
      lastVal = begin[i];
      if (resultArray_index >= 90) {
        //return;
      }
    }
  }
  
  // Error checking
  for (i = 0; i < 88; i++) { // ignore the parody bit ([88] and [89])
    if (resultArray[i] == 2) {
      return;
    }
  }
  
  //------------------------------------------
  // MANCHESTER DECODING
  //------------------------------------------
  for (i = 0; i < 88; i+=2) { // ignore the parody bit ([88][89])
    if (resultArray[i] == 1 && resultArray[i+1] == 0) {
      finalArray[finalArray_index] = 1;
    }
    else if (resultArray[i] == 0 && resultArray[i+1] == 1) {
      finalArray[finalArray_index] = 0;
    }
    else {
      // The read code is not in manchester, ignore this read tag and try again
      // free the allocated memory and end the function
      return;
    }
    finalArray_index++;
  }
  
  
  //------------------------------------------
  // SERVO MECHANICS
  //------------------------------------------
  if (searchTag(convertInput (finalArray))){
    PORTB |= 0x04; // Green Light
    
    // open the door
    OCR1A = 10000 - SERVO_OPEN;
    {
      unsigned long i;
      for (i = 0; i < 2500000; i++) {
        if (!((PINB & (1<<7))>>7)) {
          break;
        }
      }
    }
    //close the door
    OCR1A = 10000 - SERVO_CLOSE;
    {
      unsigned long i;
      for (i = 0; i < 500000; i++) {
        asm volatile ("nop");
      }
    }
    OCR1A = 0;
    wait (5000);
  }
  else {
    PORTB |= 0x08;//Red Light
    wait (5000);
  }
}


/******************************* MAIN FUNCTION *******************************\
| This is the main function, it initilized the variabls and then waits for    |
| interrupt to fill the buffer before analizing the gathered data             |
\*****************************************************************************/
int main (void) {
  int i = 0;

  //------------------------------------------
  // INITLILIZATION
  //------------------------------------------

  // Load the list of valid ID tags
  addNames(); 
  
  //==========> PIN INITILIZATION <==========//
  DDRD = 0x00; // 00000000 configure output on port D
  DDRB = 0x1E; // 00011100 configure output on port B
  
  //=========> SERVO INITILIZATION <=========//
  ICR1 = 10000;// TOP count for the PWM TIMER
  
  // Set timer on match, clear on TOP
  TCCR1A  = ((1 << COM1A1) | (1 << COM1A0));
  TCCR1B  = ((1 << CS11) | (1 << WGM13));
  
  // Move the servo to close Position
  OCR1A = 10000 - SERVO_CLOSE;
  {
    unsigned long j;
    for (j = 0; j < 500000; j++) {
      asm volatile ("nop");
    }
  }
  // Set servo to idle
  OCR1A = 0;
  
  //========> VARIABLE INITILIZATION <=======//
  count = 0;
  begin = malloc (sizeof(char)*ARRAYSIZE);
  iter = 0;
  for (i = 0; i < ARRAYSIZE; i ++) {
    begin[i] = 0;
  }
  
  //=======> INTERRUPT INITILAIZATION <======//
  sei ();       // enable global interrupts
  EICRA = 0x03; // configure interupt INT0
  EIMSK = 0x01; // enabe interrupt INT0
  
  //------------------------------------------
  // MAIN LOOP
  //------------------------------------------
  while (1) {
    //enable interrupts
    sei();
    
    // while the card is being read
    while (1) {
      // if the buffer is full
      if (iter >= ARRAYSIZE) {
        cli(); // disable interrupts
        break; // continue to analize the buffer
      }
    }  
    
    PORTB &= ~0x1C;
    
    //analize the array of input
    analizeInput ();
    
    //reset the saved values to prevent errors when reading another card
    count = 0;
    iter = 0;
    for (i = 0; i < ARRAYSIZE; i ++) {
      begin[i] = 0;
    }
  }
}
