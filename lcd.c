/*
    File:       lcd.c
    Version:    0.1.0
    Date:       Feb. 25, 2006
    
    C header file for HD44780 LCD module using a 74HCT164 serial in, parallel 
    out out shift register to operate the LCD in 8-bit mode.  Example schematic
 
    ****************************************************************************
*/

#include "lcd.h"
#include <inttypes.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
/*
 * Turns the backlight on or off.  The LCD_BACKLIGHT_PIN should be defined as
 * the pin connected to the backlight control of the LCD.
 *
 * Parameters:
 *      backlight_on    0=off, 1=on
*/

void lcd_delay_ms(unsigned int ms)/* delay for a minimum of <ms> */
{
	// we use a calibrated macro. This is more
	// accurate and not so much compiler dependent
	// as self made code.
	while(ms){
		_delay_ms(0.96);
		ms--;
	}
}




/*
 * Initializes the LCD.  Should be called during the initialization of the 
 * program.
 *
 * Parameters:
 *      set_function    See LCD_FUNCTION_* definitions in lcd.h
 *      set_entry_mode  See LCD_CMD_ENTRY_* definitions in lcd.h
 *      on              See LCD_CMD_ON_* definitions in lcd.h
 
 
*/
void
lcd_initialize(uint8_t set_function, uint8_t set_entry_mode, uint8_t on)
{
        /* 20ms delay while LCD powers on */
        _delay_ms(30);	   
        
        /* Write 0x30 to LCD and wait 5 mS for the instruction to complete */
        lcd_load_byte(0x30);
        lcd_send_cmd();
        _delay_ms(10);
        
        /* Write 0x30 to LCD and wait 160 uS for instruction to complete */
        lcd_load_byte(0x30);
        lcd_send_cmd();
        _delay_us(20);
        
        /* Write 0x30 AGAIN to LCD and wait 160 uS */
        lcd_load_byte(0x30);
        lcd_send_cmd();
        _delay_us(160);
        
        /* Set function and wait 40uS */
        lcd_load_byte(set_function);
        lcd_send_cmd();
        
        /* Turn off the display and wait 40uS */
        lcd_load_byte(LCD_CMD_OFF);    
        lcd_send_cmd();
        
        /* Clear display and wait 1.64mS */
        lcd_load_byte(LCD_CMD_CLEAR);    
        lcd_send_cmd();
        _delay_ms(4);
        
        /* Set entry mode and wait 40uS */
        lcd_load_byte(set_entry_mode);    
        lcd_send_cmd();
        _delay_ms(4);
        /* Turn display back on and wait 40uS */
        lcd_load_byte(on);    
        lcd_send_cmd();
		_delay_ms(40);
};

/*
 * Loads a byte into the shift register (74'164).  Does NOT load into the LCD.
 *
 * Parameters:
 *      out_byte        The byte to load into the '164.
*/
void 
lcd_load_byte(uint8_t out_byte)
{
        /* make sure clock is low */
        LCD_PORT &= ~_BV(LCD_CLOCK_PIN);
        
        int i;
        for(i=0; i<8; i++)
        {
                /* loop through bits */
                
                if (out_byte & 0x80)	// Maske 1000 0000
                {
                        /* this bit is high */
                        LCD_PORT |=_BV(LCD_RSDS_PIN); 
                }
                else
                {
                        /* this bit is low */
                        LCD_PORT &= ~_BV(LCD_RSDS_PIN);
                }
                out_byte = out_byte << 1;
                
                /* pulse the the shift register clock */
                LCD_PORT |= _BV(LCD_CLOCK_PIN);	
				_delay_us(40);
				//Clk des Schieberegisters
                LCD_PORT &= ~_BV(LCD_CLOCK_PIN);
        }
}

/*
 * Loads the byte in the '164 shift register into the LCD as a command. The
 * '164 should already be loaded with the data using lcd_load_byte().
*/
void
lcd_send_cmd(void)
{
        /* Data in '164 is a command, so RS must be low (0) */
        LCD_PORT &= ~_BV(LCD_RSDS_PIN); 
        lcd_strobe_E();	
        _delay_us(50);
}

/*
 * Loads the byte in the '164 shift register into the LCD as a character. The
 * '164 should already be loaded with the data using lcd_load_byte().
*/
void
lcd_send_char(void)
{
        /* Data in '164 is a character, so RS must be high (1) */
        LCD_PORT |= _BV(LCD_RSDS_PIN); 
        lcd_strobe_E();
        _delay_us(50);
}

/*
 * Loads the byte into the shift register and then sends it to the LCD as a char
 * Parameters:
 *      c               The byte (character) to display
*/
void 
lcd_putc(const char c)
{
        lcd_load_byte(c);
        lcd_send_char();
}


void lcd_putint(uint8_t zahl)
{
char string[4];
  int8_t i;                             // schleifenzähler
 
  string[3]='\0';                       // String Terminator
  for(i=2; i>=0; i--) 
  {
    string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
    zahl /= 10;
  }
lcd_puts(string);
}

void lcd_putint3(uint16_t zahl)     // int bis 1000
{
   char string[4];
   int8_t i;                             // schleifenzähler
   
   string[3]='\0';                       // String Terminator
   for(i=2; i>=0; i--) 
   {
      string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
   lcd_puts(string);
}

void lcd_putint16(uint16_t zahl)
{
   char string[8];
   int8_t i;                             // schleifenzähler
   
   string[7]='\0';                       // String Terminator
   for(i=6; i>=0; i--) 
   {
      string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
   lcd_puts(string);
}


void lcd_putint12(uint16_t zahl)
{
   char string[5];
   int8_t i;                             // schleifenzähler
   
   string[4]='\0';                       // String Terminator
   for(i=3; i>=0; i--)
   {
      string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
   lcd_puts(string);
}




void lcd_putint2(uint8_t zahl)	//zweistellige Zahl
{
	char string[3];
	int8_t i;								// Schleifenzähler
	zahl%=100;								// 2 hintere Stelle
	//  string[4]='\0';                     // String Terminator
	string[2]='\0';							// String Terminator
	for(i=1; i>=0; i--) {
		string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
		zahl /= 10;
	}
	lcd_puts(string);
}

// http://www.mikrocontroller.net/topic/156564

void lcd_writestring(const char *string)
{
   while (*string)
      lcd_putc(*string++);
}

void lcd_writeuint( uint16_t Number, uint8_t fieldWidth )
{
   char Buffer[10];
   int8_t Missing, i;
   
   utoa( Number, Buffer, 10 );
   
   Missing = fieldWidth - strlen( Buffer );
   for( i = 0; i < Missing; ++i )
      lcd_putc( ' ' );
   
   lcd_writestring( Buffer );
}


/*************************************************************************
Display string without auto linefeed 
Input:    string to be displayed
Returns:  none
*************************************************************************/
void lcd_puts(const char *s)
/* print string on lcd (no auto linefeed) */
{
    register char c;

    while ( (c = *s++) ) {
        lcd_putc(c);
    }

}/* lcd_puts */

void lcd_puthex(uint8_t zahl)
{
   //char string[5];
   char string[3];
   uint8_t l,h;                          // schleifenzähler
   
   string[2]='\0';                       // String Terminator
   l=(zahl % 16);
   if (l<10)
      string[1]=l +'0';  
   else
   {
      l%=10;
      string[1]=l + 'A'; 
      
   }
   zahl /=16;
   h= zahl % 16;
   if (h<10)
      string[0]=h +'0';  
   else
   {
      h%=10;
      string[0]=h + 'A'; 
   }
   
   
   lcd_puts(string);
}




/*
 * Strobes the E signal on the LCD to "accept" the byte in the '164.  The RS
 * line determines wheter the byte is a character or a command.
*/
void
lcd_strobe_E(void)
{
        /* strobe E signal */
        LCD_PORT |= _BV(LCD_ENABLE_PIN);
        _delay_us(400);
	//	  _delay_ms(20); 
	//	lcddelay_ms(100);
        LCD_PORT &= ~_BV(LCD_ENABLE_PIN);
}

/*
 * Moves the cursor to the home position.
*/



/*************************************************************************
Set cursor to specified position
Input:    x  horizontal position  (0: left most position)
          y  vertical position    (0: first line)
Returns:  none
*************************************************************************/
void lcd_gotoxy(uint8_t x, uint8_t y)
{
switch (y)
{
    case 0:
        lcd_load_byte((1<<LCD_DDRAM)+LCD_START_LINE1+x);
		lcd_send_cmd();
		break;
   case 1:
		lcd_load_byte((1<<LCD_DDRAM)+LCD_START_LINE2+x);
		lcd_send_cmd();
		break;
	case 2:
		lcd_load_byte((1<<LCD_DDRAM)+LCD_START_LINE3+x);
		lcd_send_cmd();
		break;
	case 3:
		lcd_load_byte((1<<LCD_DDRAM)+LCD_START_LINE4+x);
		lcd_send_cmd();
		break;
	

}//switch


}/* lcd_gotoxy */

// Display loeschen
void lcd_cls(void)   
{
	lcd_load_byte(0x01);
	lcd_send_cmd();
//	lcd_write(0x02,0);   	// B 0000 0010 => Display loeschen
	lcd_delay_ms(2);			// dauert eine Weile, Wert ausprobiert

//	lcd_write(0x01,0);   	// B 0000 0001 => Cursor Home
	lcd_load_byte(0x02);
	lcd_send_cmd();
	
	lcd_delay_ms(2);			// dauert eine Weile, Wert ausprobiert
}


// Linie Loeschen
void lcd_clr_line(uint8_t Linie)
{
	lcd_gotoxy(0,Linie);
	uint8_t i=0;
	for (i=0;i<LCD_DISP_LENGTH;i++)
	{
		lcd_putc(' ');
	}
	lcd_gotoxy(0,Linie);
	lcd_delay_ms(2);
}	// Linie Loeschen





/*
 
Funktion zur Umwandlung einer vorzeichenlosen 32 Bit Zahl in einen String
 
*/
 
void r_uitoa(uint32_t zahl, char* string) {
  int8_t i;                             // schleifenzähler
 
  string[10]='\0';                       // String Terminator
  for(i=9; i>=0; i--) {
    string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
    zahl /= 10;
  }
}


/*
 
Funktion zur Umwandlung einer vorzeichenbehafteten 32 Bit Zahl in einen String
 
*/
 
void r_itoa(int32_t zahl, char* string) 
{
  uint8_t i;
 
  string[11]='\0';                  // String Terminator
  if( zahl < 0 ) {                  // ist die Zahl negativ?
    string[0] = '-';              
    zahl = -zahl;
  }
  else string[0] = ' ';             // Zahl ist positiv
 
  for(i=10; i>=1; i--) {
    string[i]=(zahl % 10) +'0';     // Modulo rechnen, dann den ASCII-Code von '0' addieren
    zahl /= 10;
  }
}

 
void r_itoa16(int16_t zahl, char* string) 
{
  uint8_t i;
 
  string[7]='\0';                  // String Terminator
  if( zahl < 0 ) {                  // ist die Zahl negativ?
    string[0] = '-';              
    zahl = -zahl;
  }
  else string[0] = ' ';             // Zahl ist positiv
 
  for(i=6; i>=1; i--) {
    string[i]=(zahl % 10) +'0';     // Modulo rechnen, dann den ASCII-Code von '0' addieren
    zahl /= 10;
  }
}

 
void r_itoa8(int8_t zahl, char* string) 
{
  uint8_t i;
 
  string[4]='\0';                  // String Terminator
  if( zahl < 0 ) 
  {                  // ist die Zahl negativ?
    string[0] = '-';              
    zahl = -zahl;
  }
  else string[0] = ' ';             // Zahl ist positiv
 
  for(i=3; i>=1; i--) 
  {
    string[i]=(zahl % 10) +'0';     // Modulo rechnen, dann den ASCII-Code von '0' addieren
    zahl /= 10;
  }
}

void r_uitoa8(int8_t zahl, char* string) 
{
  uint8_t i;
 
  string[3]='\0';                  // String Terminator
  for(i=3; i>=0; i--) 
  {
    string[i]=(zahl % 10) +'0';     // Modulo rechnen, dann den ASCII-Code von '0' addieren
    zahl /= 10;
  }
}


/*
 
Funktion zur Anzeige einer 32 Bit Zahl im Stringformat
auf einem LCD mit HD44780 Controller
Quelle: www.mikrocontroller.net/articles/Festkommaarithmetik
Parameter:
 
char* string  : Zeiger auf String, welcher mit my_itoa() erzeugt wurde
uint8_t start : Offset im String, ab der die Zahl ausgegeben werden soll,
                das ist notwenig wenn Zahlen mit begrenztem Zahlenbereich
                ausgegeben werden sollen
                Vorzeichenlose Zahlen      : 0..10
                Vorzeichenbehaftete zahlen : 1..11
uint8_t komma : Offset im String, zeigt auf die Stelle an welcher das virtuelle
                Komma steht (erste Nachkommastelle)
                komma muss immer grösser oder gleich start sein !
 
uint8_t frac  : Anzahl der Nachkommastellen
 
*/
 
 
 


 
 





/*************************************************************************/

