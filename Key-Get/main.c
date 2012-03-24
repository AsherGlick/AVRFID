/*****************************************************************************\
| This program was written by Asher Glick, This program is provided ASIS, any | 
| complaints should be addressed to /dev/null. Any comments about how to make |
| the program better should be addressed to aglick@tetrakai.com do not get    |
| those confused. I put alot of work into this code and I enjoy working on it |
| if you have gotten you hands on this code without permission, you are lazy  |
| and should do this yourself. If this code is found online remove it         |
| instantly, it does not belong there. Due to security issues, please do not  |
| distribute this code to other people or enteties. All aspects of the RFID   |
| reader may be shared freely except for this code, thank you and enjoy       |
| a life of goodness and access                                               |
\*****************************************************************************/

/**************** OPEN SOURCE *************************************************\
| As per recent incedents, I have decided to open this code to the public.     |
| This code will now fall under the BSD licence wich you can view in ./LICENCE |
| file. Have a nice day. As of the date of writing (March 6, 2011) the source  |
| has been compleetly open sourced, however it has not been made into a format |
| that can be easily understood. The complete documentation will be released   |
| at the end of May 2011. Thanks for your support and happy hacking            |
\******************************************************************************/

/****************** CHIP SETTINGS ******************\
| This program was designed to run on an ATMEGA328  |
| chip running with an external clock at 8MHz       |
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
| NOTE: '-c usbtiny' is incorrect if you are using a different programmer      |
\******************************************************************************/

#define SERVO_OPEN 575
#define SERVO_CLOSE 1000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define ARRAYSIZE 900   // Number of point to collect each time

int * begin;            // points to the bigining of the array
int * names;
int namesize;
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
  namesize = 31;
  names = malloc (sizeof(int) * namesize);
  names [0] = 12345;
  names [1] = 12346;
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
  //Save the value of DEMOD_OUT to prevent re-reading
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

/******************************** USART CONFIG ********************************\
| USART_Init(void) initilizes the USART feature, this function needs to be run |
| before any USART functions are used, this function configures the BAUD rate  |
| for the USART and enables the format for transmission                        |
\******************************************************************************/
#define FOSC 8000000 // Clock Speed of the procesor
#define BAUD 19200    // Baud rate (to change the BAUD rate change this variable
#define MYUBRR FOSC/16/BAUD-1 // calculate the number the processor needs
void USART_Init(void) {
  unsigned int ubrr = MYUBRR;
  /*Set baud rate */
  UBRR0H = (unsigned char)(ubrr>>8);
  UBRR0L = (unsigned char)ubrr;
  /*Enable receiver and transmitter */
  UCSR0B = (1<<RXEN0)|(1<<TXEN0);
  /* Set frame format: 8data, 2stop bit */
  UCSR0C = (1<<USBS0)|(3<<UCSZ00);
}

/******************************* USART_Transmit *******************************\
| The USART_Transmit(int) function allows you to send numbers to USART serial  |
| This function only handles numbers up to two digits. If there is one digit   |
| the message contains a space, then the digit converted to ascii. If there    |
| are two digits then the message is the first digit followed by the seccond   |
| If the input is negative then the function will output a newline character   |
\******************************************************************************/
void USART_Transmit( int input )
{
  unsigned char data;
  if (input == -1) {
    while ( !( UCSR0A & (1<<UDRE0)) );
    // Put '\n' into the bufffer to send
    UDR0 = '\r';
    //dont continue running the function to prevent outputing E
    // Wait for empty transmit buffer
    while ( !( UCSR0A & (1<<UDRE0)) );
    // Put '\n' into the bufffer to send
    UDR0 = '\n';
    //dont continue running the function to prevent outputing E
    return;
  }
  else if (input < 10 && input >= 0) {     
    while ( !( UCSR0A & (1<<UDRE0)) );
    data = '0' + input;
    UDR0 = data;
  }
  else {
    // Output E if the number cannot be outputed
    UDR0 = 'E';
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
  if (array[28]) result += 32768; //0
  if (array[29]) result += 16384; //1
  if (array[30]) result += 8192;  //0
  if (array[31]) result += 4096;  //0
  if (array[32]) result += 2048;  //1
  if (array[33]) result += 1024;  //0
  if (array[34]) result += 512;   //1
  if (array[35]) result += 256;   //1
  if (array[36]) result += 128;   //0
  if (array[37]) result += 64;    //0
  if (array[38]) result += 32;    //1
  if (array[39]) result += 16;    //0
  if (array[40]) result += 8;     //0
  if (array[41]) result += 4;     //1
  if (array[42]) result += 2;     //0
  if (array[43]) result += 1;     //0
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
| 1) Converts raw data (3,4,5,6,7,8,9,10) to binary data (0,1) errors have    |
|     values of 99 instead of 1 or 0                                          |
| 2) Finds a start tag in the code                                            |
| 3) Parses the data from multibit code (11111000000000000111111111100000) to |
|     singlebit manchester code (100110) untill it finds an end tag           |
| 4) Converts manchester code (100110) to binary code (010)                   |
\*****************************************************************************/
void analizeInput (void) {
  int i;              // Generic for loop 'i' counter
  int inARow = 0;     // number of identical bits in a row
  int lastVal = 0;    // value of the identical bits in a row
  int resultArray[90];  // Parsed Bit code in manchester
  int resultArray_index = 0;
  int finalArray[45];   //Parsed Bit Code out of manchester
  int finalArray_index = 0;
  
  // Initilize the arrays to 2 so that any errors show up as 2s
  for (i = 0; i < 90; i ++) {
    resultArray[i] = 2;
  }
  for (i = 0; i < 45; i++) {
    finalArray[i] = 2;
  }
  
  
  // uncomment this line ( (/*)->(//*) ) to displa
  /*
  USART_Transmit(0);
  USART_Transmit(0);
  USART_Transmit(0);
  for (i = 0; i < ARRAYSIZE; i++) {
    USART_Transmit(begin[i]);
  }
  // Send newline
  USART_Transmit(-1);
  /**/
  
  // Convert raw data to 1, 0, and error (99)
  #ifdef distance_far
  // far range will allow you to read the card from
  // farther away and probably still be as accurate
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
  #endif
  /*
  for (i = 0; i < ARRAYSIZE; i++) {
    USART_Transmit(begin[i]);
  }
  // Send newline
  USART_Transmit(-1);
  /**/
  // Find Start Tag
  
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
  // PARSE TO BIT DATA
  
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
  for (i = 0; i < 88; i++) { // ignore the parody bit ([88][89])
    if (resultArray[i] == 2) {
      return;
    }
  }
  
  // MANCHESTER DECODING
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
  
  // Send final code over serial
  //*
  for (i = 0; i < 45; i ++) {
    USART_Transmit(finalArray[i]);
  }
  // Send newline
  USART_Transmit(-1);
  /**/
  if (searchTag(convertInput (finalArray))){
    PORTB |= 0x04;
    // open the door
    OCR1A = 10000 - SERVO_OPEN;
  {
    unsigned long i;
    for (i = 0; i < 2500000; i++) {
      //asm volatile ("nop");
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
    PORTB |= 0x08;
    wait (5000);
  }
}


/******************************* MAIN FUNCTION *******************************\
|
\*****************************************************************************/
int main (void) {
  addNames(); // Load the list of valid ID tags
  int i = 0;
  
  // PIN INITILIZATION
  DDRD = 0x00;// 00000000 configure output on port D
  DDRB = 0x1E;// 00011100 configure output on port B
  
  // SERVO INITILIZATION
  ICR1 = 10000;// TOP count for the PWM TIMER
  // Set on match, clear on TOP
  TCCR1A  = ((1 << COM1A1) | (1 << COM1A0));
  TCCR1B  = ((1 << CS11) | (1 << WGM13));
  OCR1A = 10000 - SERVO_CLOSE;
  {
    unsigned long j;
    for (j = 0; j < 500000; j++) {
      asm volatile ("nop");
    }
  }
  OCR1A = 0;
  
  //VARIABLE INITILIZATION
  count = 0;
  begin = malloc (sizeof(char)*ARRAYSIZE);
  iter = 0;
  for (i = 0; i < ARRAYSIZE; i ++) {
    begin[i] = 0;
  }
  
  // USART INITILIZATION
  USART_Init();
  
  //INTERRUPT INITILAIZATION
  sei ();       // enable global interrupts
  EICRA = 0x03; // configure interupt INT0
  EIMSK = 0x01; // enabe interrupt INT0
  //MAIN LOOP
  while (1) {
    //enable interrupts
    sei();
    // while the card is being read
    while (1) {
      if (iter >= ARRAYSIZE) {// when the card is finished being read
        cli(); // disable interrupts
        break;
      }
    }  
    
    //analize the array of input
    PORTB &= ~0x1C;
    analizeInput ();
    //reset the card reader so it can read another card
    count = 0;
    iter = 0;
    for (i = 0; i < ARRAYSIZE; i ++) {
      begin[i] = 0;
    }
  }
}
