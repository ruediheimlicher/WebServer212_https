//
//  defines.h
//  WebServer
//
//  Created by Ruedi Heimlicher on 23.07.2016.
//
//

#ifndef defines_h
#define defines_h

#define TEST 1

#define LOOPLED_PORT  PORTD
#define LOOPLED_DDR   DDRD
#define LOOPLED         7


// set output to VCC, red LED off
#define LEDOFF PORTB|=(1<<PORTB1)
// set output to GND, red LED on
#define LEDON PORTB&=~(1<<PORTB1)
// to test the state of the LED
#define LEDISOFF PORTB&(1<<PORTB1)
// packet buffer
#define BUFFER_SIZE 800



//

// set output to VCC, red LED off
#define PD6LEDOFF PORTD|=(1<<PORTD0)
// set output to GND, red LED on
#define PD6LEDON PORTD&=~(1<<PORTD0)
// to test the state of the LED
#define PD6LEDISOFF PORTD&(1<<PORTD6)
//

static char* teststring = "pw=Pong&strom0=360\0";

// listen port for tcp/www:
//#define MYWWWPORT 1401
#define MYWWWPORT 5000

#define UPLOADMINUTEN 1



#endif /* defines_h */
