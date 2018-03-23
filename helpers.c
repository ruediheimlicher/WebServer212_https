//
//  helpers.c
//  WebServer
//
//  Created by Ruedi Heimlicher on 23.07.2016.
//
//

#include <stdio.h>
#include "helpers.h"
#include "defines.h"

void lcdinit()
{
   //*********************************************************************
   //	Definitionen LCD im Hauptprogramm
   //	Definitionen in lcd.h
   //*********************************************************************
   
   LCD_DDR |= (1<<LCD_RSDS_PIN); //PIN 3 von PORT D als Ausgang fuer LCD_RSDS_PIN
   LCD_DDR |= (1<<LCD_ENABLE_PIN); //PIN 4 von PORT D als Ausgang fuer LCD_ENABLE_PIN
   LCD_DDR |= (1<<LCD_CLOCK_PIN); //PIN 5 von PORT D als Ausgang fuer LCD_CLOCK_PIN
   /* initialize the LCD */
   lcd_initialize(LCD_FUNCTION_8x2, LCD_CMD_ENTRY_INC, LCD_CMD_ON);
   lcd_puts("LCD Init\0");
   _delay_ms(300);
   lcd_cls();
   
}
