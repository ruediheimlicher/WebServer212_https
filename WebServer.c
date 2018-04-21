/*********************************************
 * vim:sw=8:ts=8:si:et
 * To use the above modeline in vim you must have "set modeline" in your .vimrc
 * Author: Guido Socher
 * Copyright: GPL V2
 * See http://www.gnu.org/licenses/gpl.html
 *
 * This software implements a  web browser.
 * The the browser tries to access 
 * the webserver at WEBSERVER_VHOST when a push button is pressed
 * It will upload data to http://tuxgraphics.org/cgi-bin/upld
 * using http-get.
 *
 * When you connect PD6 with GND then it will upload a line to the 
 * upld page that should look like this:
 * 70.80.220.56 :: 2012-01-28 18:08 GMT :: action=btn :: ethbrd_ip=10.0.0.30 :: pw=sec ::
 *
 * There are two LEDs you can connect to the circuit to be able to
 * see what is going on.
 *
 * LED1 between PB1 and Vcc: this LED will be off during transations and go
 * back on when the transation is finished. Transations are e.g: get IP 
 * via dhcp, resolve tuxgraphics.org via DNS, start a browswe to upload
 * data to http://tuxgraphics.org/cgi-bin/upld
 *
 * LED2 between PD0 and Vcc: this LED will only go on if the DHCP server
 * did not provide a gateway IP address. This software needs absolutely
 * a gateway IP address and can not function without it.
 *
 * Chip type           : Atmega168 or Atmega328 or Atmega644 with ENC28J60
 *********************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "ip_arp_udp_tcp.c"
#include "websrv_help_functions.c"
#include "enc28j60.c"


#include "timeout.h"
#include "net.h"
#include "dnslkup.c"
#include "dhcp_client.c"

#include "lcd.c"
#include "defines.h"
#include "helpers.c"
#include "datum.c"
#include "version.c"
#include "webpage.c"
#include "current.c"

//
// Please modify the following lines. mac and ip have to be unique
// in your local area network. You can not have the same numbers in
// two devices:
static uint8_t mymac[6] = {0x54,0x55,0x58,0x10,0x00,0x99};
// how did I get the mac addr? Translate the first 3 numbers into ascii is: TUX
//
// The name of the virtual host which you want to 
// contact at websrvip (hostname of the first portion of the URL):

#define WEBSERVER_VHOST "www.ruediheimlicher.ch"

#define LOCAL_VHOST "localhost/public_html"

static uint8_t otherside_www_ip[4];
// My own IP (DHCP will provide a value for it):
// DHCP
//static uint8_t myip[4]={0,0,0,0};
// Default gateway (DHCP will provide a value for it):
//static uint8_t gwip[4]={0,0,0,0};

static uint8_t myip[4] = {192,168,1,212};
static uint8_t gwip[4] = {192,168,1,1};
static char dhcpstring[128]; // string fuer Daten zu dhcp.pl
//static char dhcpstringsauber[128];
static char pingstring[64]; // string fuer ping-antwort zu dhcp.pl


#define TRANS_NUM_GWMAC 1
static uint8_t gwmac[6];
// Netmask (DHCP will provide a value for it):
static uint8_t netmask[4];
static char urlvarstr[30];
static volatile uint8_t sec=0;      // counts up to 6 and goes back to zero
static volatile uint8_t gsec=0;     // counts up beyond 6 sec
static volatile uint16_t dhcpsec=0;  // zaehler fuer erneuten Check von otherside_ip
static volatile uint8_t min=0;

#define DHCPDELAY          300      // delay bis zum erneuten Check von otherside_ip
static uint8_t buf[BUFFER_SIZE+1];
static uint8_t start_web_client=0;
static uint8_t pingsrcip[4];
static uint8_t web_client_attempts=0;
static uint8_t web_client_sendok=0;
static uint8_t web_client_send_err=0;

static uint8_t linkuperr=0;

volatile uint8_t isrcounter=0;
volatile uint8_t taskcounter=0;
volatile uint16_t sekundendiff=0;

volatile uint16_t sekundencounter=0;
volatile uint16_t oldsekunde=0;
volatile uint8_t loopstatus=0;
#define SEKUNDEN_BIT 7



static volatile uint8_t cnt2step=0;
static int8_t dns_state=0;
static int8_t gw_arp_state=0;




static uint8_t client_status=0; // Flags fuer senden von Data an Webserver
#define SENDDEFAULTPAGE 1
#define SENDSTROMPAGE 2

static volatile uint8_t ping_callback_count=0; // Anzahl pings seit neustart
static volatile uint8_t browser_callback_count=0;// Anzahl erfolgreiche Aufrufe der website seit neustart

#define STARTDELAYBIT		0
#define WDTBIT             7
#define TASTATURPIN			0           //	Eingang fuer Tastatur
#define ECHOPIN            5           //	Ausgang fuer Impulsanzeige
#define ANALOGPIN 6
#define MASTERCONTROLPIN	4           // Eingang fuer MasterControl: Meldung MasterReset

#define INT0PIN            2           // Pin Int0
#define INT1PIN            3           // Pin Int1
#define TIMERIMPULSDAUER            10    // us
#define CURRENTSEND                 0     // Daten an server senden
#define CURRENTSTOP                 1     // Bit fuer: Impulse ignorieren
#define CURRENTWAIT                 2     // Bit fuer: Impulse wieder bearbeiten
#define CURRENTMESSUNG              3     // Bit fuer: Messen
#define DATASEND                    4     // Bit fuer: Daten enden
#define DATAOK                      5     // Data kan gesendet weren
#define DATAPEND                    6     // Senden ist pendent
#define DATALOOP                    7     // wird von loopcount1 gesetzt, startet senden

#define TIMER0_SEKUNDENTAKT            50000           // Anz Overflows von timer0 bis 1 Sekunde


static volatile uint16_t timer0counter=0;
#define SEKUNDEN_BIT 7
volatile uint8_t                    intervallsekunden =0;     // Byte fuer errors der Messung/Uebertragung


volatile uint16_t                   wattstunden=0;
volatile uint16_t                   kilowattstunden=0;
volatile uint32_t                   webleistung=0;

volatile float leistung =1.0;
float lastleistung =1;
uint8_t lastcounter=0;
volatile uint8_t  anzeigewert =0;

static char stromstring[10];
static char CurrentDataString[64];

// *************** von WebServer big
// the password string (only the first 5 char checked), (only a-z,0-9,_ characters):
static char password[10]="ideur00"; // must not be longer than 9 char


// *************** end WebServer big

// http://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
char *trimwhitespace(char *str)
{
   char *end;
   
   // Trim leading space
   while(isspace(*str)) str++;
   
   if(*str == 0)  // All spaces?
      return str;
   
   // Trim trailing space
   end = str + strlen(str) - 1;
   while(end > str && isspace(*end)) end--;
   
   // Write new null terminator
   *(end+1) = 0;
   
   return str;
}


void initOSZI(void)
{
   OSZIPORTDDR |= (1<<PULS);
   OSZIPORT |= (1<<PULS); // HI
}

void timer0(void) // Analoganzeige Messinstrument
{
   //----------------------------------------------------
   // Set up timer 0
   //----------------------------------------------------
   /*
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS00) | _BV(CS02);
    OCR0A = 0x2;
    TIMSK0 = _BV(OCIE0A);
    */
   
   DDRD |= (1<< PD6);   // OC0A Output
   
   TCCR0A |= (1<<WGM00);   // fast PWM  top = 0xff
   TCCR0A |= (1<<WGM01);   // PWM
   //TCCR0A |= (1<<WGM02);   // PWM
   
   TCCR0A |= (1<<COM0A1);   // set OC0A at bottom, clear OC0A on compare match
  /*
   0   0   0  No clock source (Timer/Counter stopped)
   0   0   1   clkI/O/(No prescaling)
   0   1   0   clkI/O/8 (From prescaler)
   0   1   1   clkI/O/64 (From prescaler)   
   1   0   0   clkI/O/256 (From prescaler)
   1   0   1   clkI/O/1024 (From prescaler)
   */
   // clk/1024
   TCCR0B |= 1<<CS02;
   TCCR0B |= 1<<CS00;
   
   OCR0A=0;
   TIMSK0 |= (1<<OCIE0A);
   
   
}

#pragma mark ISR TIMER0_COMPA
ISR(TIMER0_COMPA_vect) // PWM fuer Analog-Anzeige.
{
   
   OCR0A = anzeigewert; // neuer Wert fuer Impulsdauer
   //OCR0A++;
   PORTD &= ~(1<<ANALOGPIN); //Ausgang LO
   
   
}

#pragma mark ISR TIMER0_OVF
ISR(TIMER0_OVF_vect) // Takt fuer PWM Analoganzeige
{
   timer0counter++;
   if (timer0counter== TIMER0_SEKUNDENTAKT) // 50000,  Anz Overflows von timer0 bis 1 Sekunde
   {
      OSZITOGG;
      sekundencounter++; // verwendet fur min
      if (sekundencounter%60 == 0)
      {
         
         min++;
      }
      timer0counter=0;
   }
   PORTD |= (1<<ANALOGPIN);//Ausgang HI
}

#pragma mark ISR TIMER1_COMPA
// eth_dhcp: timer interrupt, called automatically every second
ISR(TIMER1_COMPA_vect)
{
   sec++;
   gsec++;
   
   if (sec>5) // refresh, DHCP erfordert periodischen refresh
   {
      sec=0;
 //     dhcp_6sec_tick(); // inkkrementiert dhcp_6sec_cnt
   }
   
   dhcpsec++;
   if (dhcpsec > DHCPDELAY)
   {
      dhcpsec=0;
   }
   
   sekundencounter++; // verwendet fur min
   loopstatus |= (1<<SEKUNDEN_BIT);
   
   taskcounter++;
   
   if (taskcounter>= 8)
   {
      taskcounter=0;
      min++;
     // if (min >= UPLOADMINUTEN)
      {
         min=0;
         //webstatus |= (1<<DATAOK);
        // if (!(webstatus & (1<<CURRENTSEND))) // noch nicht gesetzt
         {
            
            // ******** prov, setzt pl in Gang
            
            // ********
            webstatus |= (1<<CURRENTSEND);
         }
      }
      
   }
}


// Generate an interrup about ever 1s form the 12.5MHz system clock
// Since we have that 1024 prescaler we do not really generate a second
// (1.00000256000655361677s) 
void timer1_init(void)
{
        /* write high byte first for 16 bit register access: */
        TCNT1H=0;  /* set counter to zero*/
        TCNT1L=0;
        // Mode 4 table 14-4 page 132. CTC mode and top in OCR1A
        // WGM13=0, WGM12=1, WGM11=0, WGM10=0
        TCCR1A=(0<<COM1B1)|(0<<COM1B0)|(0<<WGM11);
        TCCR1B=(1<<CS12)|(1<<CS10)|(1<<WGM12)|(0<<WGM13); // crystal clock/1024

        // At what value to cause interrupt. You can use this for calibration
        // of the clock. Theoretical value for 12.5MHz: 12207=0x2f and 0xaf
        OCR1AH=0x2f;
        OCR1AL=0xaf;
        // interrupt mask bit:
        TIMSK1 = (1 << OCIE1A);
}


// von test_www_client
uint16_t http200ok(void)
{
   return(fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n")));
}


// prepare the webpage by writing the data to the tcp send buffer
uint16_t print_webpage(uint8_t *buf)
{
   uint16_t plen;
   char vstr[17];
   uint8_t err;
   plen=http200ok();
   plen=fill_tcp_data_p(buf,plen,PSTR("<h2>dhcp web client status home</h2>\n<pre>\n"));
   if (gw_arp_state==1)
   {
      plen=fill_tcp_data_p(buf,plen,PSTR("waiting for GW "));
      mk_net_str(vstr,gwip,4,'.',10);
      plen=fill_tcp_data(buf,plen,vstr);
      plen=fill_tcp_data_p(buf,plen,PSTR(" to answer arp.\n"));
      return(plen);
   }
   if (dns_state==1)
   {
      plen=fill_tcp_data_p(buf,plen,PSTR("waiting for DNS answer.\n"));
      err=dnslkup_get_error_info();
      plen=fill_tcp_data_p(buf,plen,PSTR("Error code: "));
      itoa(err,vstr,10);
      plen=fill_tcp_data(buf,plen,vstr);
      plen=fill_tcp_data_p(buf,plen,PSTR(" (0=no error)\n"));
      return(plen);
   }
   plen=fill_tcp_data_p(buf,plen,PSTR("Number of data uploads: web_client_attempts: "));
   // convert number to string:
   itoa(web_client_attempts,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   
   plen=fill_tcp_data_p(buf,plen,PSTR("\nNumber of successful data uploads to web: web_client_sendok: "));
   // convert number to string:
   itoa(web_client_sendok,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);

   plen=fill_tcp_data_p(buf,plen,PSTR("\nNumber of errors to uploads to web: web_client_send_err: "));
   // convert number to string:
   itoa(web_client_send_err,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);

   
   //otherside_www_ip
   plen=fill_tcp_data_p(buf,plen,PSTR("\notherside_www_ip:\n "));
   
   // convert el 0 to string:
   itoa(otherside_www_ip[0],vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\t"));
   // convert el 1 to string:
   itoa(otherside_www_ip[1],vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\t"));
   // convert el 2 to string:
   itoa(otherside_www_ip[2],vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\t"));
   // convert el 3 to string:
   itoa(otherside_www_ip[3],vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   
   //pingsrcip:
   plen=fill_tcp_data_p(buf,plen,PSTR("\npingsrcip:\n "));
   
   // convert el 0 to string:
   itoa(pingsrcip[0],vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\t"));
   // convert el 1 to string:
   itoa(pingsrcip[1],vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\t"));
   // convert el 2 to string:
   itoa(pingsrcip[2],vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\t"));
   // convert el 3 to string:
   itoa(pingsrcip[3],vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   
   // sec:
   plen=fill_tcp_data_p(buf,plen,PSTR("\nsec:\t "));
   
   // convert el 0 to string:
   itoa(sec,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\t"));
   
   // web_client_sendok
   plen=fill_tcp_data_p(buf,plen,PSTR("\nweb_client_sendok:\t "));
   
   // convert web_client_sendok to string:
   itoa(web_client_sendok,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\t"));
   
   //ping_callback_count
   plen=fill_tcp_data_p(buf,plen,PSTR("\nping_callback_count:\t "));
   
   itoa(ping_callback_count,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\t"));
   
   plen=fill_tcp_data_p(buf,plen,PSTR("\nbrowser_callback_count:\t "));
   itoa(browser_callback_count,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\nHost: "));
   
   plen=fill_tcp_data(buf,plen,(char*)WEBSERVER_VHOST);
   plen=fill_tcp_data_p(buf,plen,PSTR("\n"));
   
   // gw_arp_state
   plen=fill_tcp_data_p(buf,plen,PSTR("\ngw_arp_state:\t "));
   itoa(gw_arp_state,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   // dns_state
   plen=fill_tcp_data_p(buf,plen,PSTR("\tdns_state:\t "));
   itoa(dns_state,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   plen=fill_tcp_data_p(buf,plen,PSTR("\n"));
   // start_web_client
   plen=fill_tcp_data_p(buf,plen,PSTR("start_web_client:\t "));
   itoa(start_web_client,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);
   
   // Leistung
   plen=fill_tcp_data_p(buf,plen,PSTR("\nleistung:\t "));
   itoa(webleistung,vstr,10);
   plen=fill_tcp_data(buf,plen,vstr);

   
   // plen=fill_tcp_data_p(buf,plen,PSTR("\ncheck result: <a href=http://tuxgraphics.org/cgi-bin/upld>http://tuxgraphics.org/cgi-bin/upld</a>"));
   
   plen=fill_tcp_data_p(buf,plen,PSTR("\n</pre><br><hr>"));
   return(plen);
}
/*
void initTimer0(void)
{
   PRR&=~(1<<PRTIM0); // write power reduction register to zero
   TIMSK0=(1<<OCIE0A); // compare match on OCR2A
   TCNT0=0;  // init counter
   OCR0A=244; // value to compare against
   TCCR0A=(1<<WGM01); // do not change any output pin, clear at compare match
  
   // CS22	CS21	CS20	Description
   //  0    0     0     No clock source
  //   0    0     1     clk/1
  //   0    1     0     clk/8
  //   0    1     1     clk/32
  //   1    0     0     clk/64
  //   1    0     1     clk/128
  //   1    1     0     clk/256
  //   1    1     1     clk/1024
   
   // divide clock by 1024: 12.5MHz/128=12207 Hz
   TCCR0B=(1<<CS02)|(1<<CS00); // clock divider, start counter // anders als timer2
   // OCR0A=244: 12207/244=50Hz
}
ISR(TIMER0_COMPA_vect)
{
   cnt2step++;
   if (cnt2step>50)
   {
      cnt2step=0;
      sec++; // stepped every second
   }
}
*/
// ******************************************
// ******* conflict with timer2 in current
// ******************************************


// setup timer T2 as an interrupt generating time base.
// * You must call once sei() in the main program

 /*

void init_cnt2(void)
{
   cnt2step=0;
   PRR&=~(1<<PRTIM2); // write power reduction register to zero
   TIMSK2=(1<<OCIE2A); // compare match on OCR2A
   TCNT2=0;  // init counter
   OCR2A=244; // value to compare against
   TCCR2A=(1<<WGM21); // do not change any output pin, clear at compare match
   // divide clock by 1024: 12.5MHz/128=12207 Hz
   TCCR2B=(1<<CS22)|(1<<CS21)|(1<<CS20); // clock divider, start counter
   // 12207/244=50Hz
}

// called when TCNT2==OCR2A
// that is in 50Hz intervals
ISR(TIMER2_COMPA_vect)
{
   cnt2step++;
   if (cnt2step>50)
   {
      cnt2step=0;
   //   sec++; // stepped every second
  //    lcd_gotoxy(16,3);
  //    lcd_putint(sec);
   }
}
*/
// ******************************************
// ******************************************

// we were ping-ed by somebody, store the ip of the ping sender
// and trigger an upload to http://tuxgraphics.org/cgi-bin/upld
// This is just for testing and demonstration purpose
void ping_callback(uint8_t *ip)
{
   uint8_t i=0;
   ping_callback_count++;
   // trigger only first time in case we get many ping in a row:
//   if (start_web_client==0)
   {
      lcd_gotoxy(16,3);
      lcd_puts("   \0");
      lcd_gotoxy(16,3);
      lcd_puts("pOK\0");

      start_web_client=1;
      // save IP from where the ping came:
      while(i<4)
      {
         pingsrcip[i]=ip[i];
         i++;
      }
   }
}

// the __attribute__((unused)) is a gcc compiler directive to avoid warnings about unsed variables.
void browserresult_callback(uint16_t webstatuscode,uint16_t datapos , uint16_t len )
{
   browser_callback_count++;
   //return;
   if (webstatuscode==200)
   {
      //lcd_gotoxy(16,3);
      //lcd_puts("   \0");
      lcd_gotoxy(16,3);
      lcd_puts("bOK\0");

      web_client_sendok++;
      webstatus &= ~(1<<CALLBACKWAIT);
      
   }
   else
   {
      
      //lcd_gotoxy(16,3);
      //lcd_puts("   \0");
      lcd_gotoxy(16,3);
      lcd_puts("bEr\0");
      web_client_send_err++;
   }
   sekundendiff = sekundencounter - oldsekunde;
   oldsekunde = sekundencounter;
 
}

// the __attribute__((unused)) is a gcc compiler directive to avoid warnings about unsed variables.
void arpresolver_result_callback(uint8_t *ip __attribute__((unused)),uint8_t transaction_number,uint8_t *mac)
{
   uint8_t i=0;
   lcd_gotoxy(16,3);
   lcd_puts("   \0");
   lcd_gotoxy(16,3);
   lcd_puts("rOK\0");
   //lcd_puthex(web_client_sendok);
   //web_client_sendok++;

   if (transaction_number==TRANS_NUM_GWMAC)
   {
      // copy mac address over:
      while(i<6){gwmac[i]=mac[i];i++;}
   }
}

// end von test_www_client

// callbacks von current

void strom_browserresult_callback(uint8_t statuscode,uint16_t datapos)
{
   // datapos is not used in this example

   if (statuscode==0)
   {
      
      lcd_gotoxy(12,3);
      lcd_puts("           \0");
      lcd_gotoxy(12,3);
      lcd_putint(statuscode);
      lcd_puts("s cb OK \0");
      
      // lcd_gotoxy(19,0);
      // lcd_putc(' ');
      lcd_gotoxy(19,0);
      lcd_putc('+');
      
      webstatus &= ~(1<<DATASEND); // Datasend auftrag ok
      
      webstatus &= ~(1<<DATAPEND); // Quittung fuer callback-erhalt
      
      // Messungen wieder starten
      
      webstatus &= ~(1<<CURRENTSTOP); // Messung wieder starten
      webstatus |= (1<<CURRENTWAIT); // Beim naechsten Impuls Messungen wieder starten
      sei();
      
      web_client_sendok++;
      
   }
   else
   {
      
      lcd_gotoxy(0,3);
      lcd_puts("           \0");
      lcd_gotoxy(0,3);
      lcd_puts("b cb err \0");
      lcd_puthex(statuscode);
      
      lcd_gotoxy(19,0);
      lcd_putc(' ');
      lcd_gotoxy(19,0);
      lcd_putc('-');
   }
}

void home_browserresult_callback(uint8_t statuscode,uint16_t datapos)
{
   // datapos is not used in this example
   if (statuscode==0)
   {
      lcd_gotoxy(0,0);
      lcd_puts("        \0");
      lcd_gotoxy(0,0);
      lcd_puts("h cb OK\0");
      web_client_sendok++;
      //				sei();
      
   }
   else
   {
      lcd_gotoxy(0,0);
      lcd_puts("        \0");
      lcd_gotoxy(0,0);
      lcd_puts("h cb err\0");
      lcd_puthex(statuscode);
      
   }
}



void alarm_browserresult_callback(uint8_t statuscode,uint16_t datapos)
{
   // datapos is not used in this example
   if (statuscode==0)
   {
      lcd_gotoxy(12,0);
      lcd_puts("        \0");
      lcd_gotoxy(0,0);
      lcd_puts("a cb OK\0");
      
      web_client_sendok++;
      //				sei();
      
   }
   else
   {
      lcd_gotoxy(0,0);
      lcd_puts("         \0");
      lcd_gotoxy(0,0);
      lcd_puts("a cb err\0");
      lcd_puthex(statuscode);
      
   }
}



int main(void)
{
   
   uint16_t loopcount0=0;
   uint16_t loopcount1=0;
   LOOPLED_DDR |= (1<<LOOPLED);
   uint16_t dat_p,plen;
   char str[20];
   char dhcpstr[20]; // fuer tempdaten bei dhcp-transfer

   uint8_t rval;
   
   DDRB|= (1<<DDB1); // LED, enable PB1 as output
   LEDON;
   DDRD|= (1<<DDD0); // second LED, enable PD0 as output
   
   //PD0LEDOFF;
   // Set the clock speed to "no pre-scaler" (8MHz with internal osc or
   // full external speed)
   // set the clock prescaler. First write CLKPCE to enable setting
   // of clock the next four instructions.
   // Note that the CKDIV8 Fuse determines the initial
   // value of the CKKPS bits.
   CLKPR=(1<<CLKPCE); // change enable
   CLKPR=0; // "no pre-scaler"
   _delay_loop_1(0); // 60us
   
   /*initialize enc28j60*/
   enc28j60Init(mymac);
   enc28j60clkout(2); // change clkout from 6.25MHz to 12.5MHz
   _delay_loop_1(0); // 60us
   
   timer1_init();
   
   // von eth
   //init_cnt2();
   
   sei();
   
   /* Magjack leds configuration, see enc28j60 datasheet, page 11 */
   // LEDB=yellow LEDA=green
   //
   // 0x476 is PHLCON LEDA=links status, LEDB=receive/transmit
   // enc28j60PhyWrite(PHLCON,0b0000 0100 0111 01 10);
   enc28j60PhyWrite(PHLCON,0x476);
   
   // PD6 the push button:
   //DDRD&= ~(1<<PIND6);
   //PORTD|=1<<PIND6; // internal pullup resistor on
   
    /* ************* DHCP Handling ***********
   LEDON;
  
   // DHCP handling. Get the initial IP
   rval=0;
   init_mac(mymac);
   while(rval==0){
      plen=enc28j60PacketReceive(BUFFER_SIZE, buf);
      buf[BUFFER_SIZE]='\0';
      rval=packetloop_dhcp_initial_ip_assignment(buf,plen,mymac[5]);
   }
   // we have an IP:
   dhcp_get_my_ip(myip,netmask,gwip);
   client_ifconfig(myip,netmask);
   LEDOFF;
   
   if (gwip[0]==0){
      // we must have a gateway returned from the dhcp server
      // otherwise this code will not work
      PD0LEDON; // error
      while(1); // stop here
   }
   
   LEDON;
   // we have a gateway.
   // find the mac address of the gateway (e.g your dsl router).
   get_mac_with_arp(gwip,TRANS_NUM_GWMAC,&arpresolver_result_callback);
   while(get_mac_with_arp_wait()){
      // to process the ARP reply we must call the packetloop
      plen=enc28j60PacketReceive(BUFFER_SIZE, buf);
      packetloop_arp_icmp_tcp(buf,plen);
   }
   LEDOFF;
   
     ************* End DHCP Handling *********** */
   
   //init the web server ethernet/ip layer:
   
   init_udp_or_www_server(mymac,myip);
   www_server_port(MYWWWPORT);

   // register to be informed about incomming ping:
   register_ping_rec_callback(&ping_callback);

   // ***** eigene Declarations ***********
   lcdinit();
   lcd_puts("Guten Tag \0");
   lcd_gotoxy(10,0);
   lcd_puts("V:\0");
   lcd_puts(VERSION);
   lcd_gotoxy(0,1);
   
   mk_net_str(str,myip,4,'.',10);
  // lcd_clr_line(1);
 //  lcd_puts(str);
   
   /*
    char versionnummer[7];
    strcpy(versionnummer,&VERSION[13]);
    versionnummer[6] = '\0';
    lcd_puts(versionnummer);
    */
   
   // set the clock speed to "no pre-scaler" (8MHz with internal osc or
   // full external speed)
   // set the clock prescaler. First write CLKPCE to enable setting of clock the
   // next four instructions.
   CLKPR=(1<<CLKPCE); // change enable
   CLKPR=0; // "no pre-scaler"
   _delay_loop_1(0); // 60us
   
   /*initialize enc28j60*/
   enc28j60Init(mymac);
   enc28j60clkout(2); // change clkout from 6.25MHz to 12.5MHz
   _delay_loop_1(0); // 60us

   
   _delay_ms(1600);
//   initOSZI();
  
   // current init
   //InitCurrent();
   //timer2();
   static volatile uint8_t paketcounter=0;
   
   uint8_t i=0;

   // ***** end eigene Declarations ***********
   lcd_clr_line(0);
   lcd_clr_line(1);
   #pragma mark while
  
   
   start_web_client=2;
   
   while(1)
   {
      if (loopstatus & (1<<SEKUNDEN_BIT))
      {
         loopstatus &= ~(1<<SEKUNDEN_BIT);
         lcd_gotoxy(15,0);
         lcd_putint12(sekundencounter);
         
         /*
         //dns_state
         lcd_gotoxy(0,1);
         lcd_puts("dn");
         lcd_puthex(dns_state);
         lcd_putc(' ');
         lcd_puts("ap");
         lcd_puthex(gw_arp_state);
         lcd_putc(' ');
         */
         //lcd_gotoxy(0,3);
         //lcd_putint(web_client_attempts);
         //uint16_t sekundendiff = sekundencounter - oldsekunde;
         //lcd_putint12(sekundendiff);
         //lcd_putc(' ');
         //oldsekunde = sekundencounter;
         //lcd_gotoxy(8,3);
         //lcd_putint(browser_callback_count);
         //lcd_putc(' ');
         //lcd_putint(web_client_attempts);

      }
      loopcount0++;
      if (loopcount0>=0x200)
      {
         
         //lcd_gotoxy(10,0);
         //lcd_putc('i');
         //lcd_putint(isrcounter);
         lcd_gotoxy(0,2);
         lcd_putc('b');
         lcd_putint(browser_callback_count);
         lcd_putc('w');
         lcd_putint(web_client_sendok);
         lcd_putc('s');
         lcd_putint(sendWebCount);
         
         lcd_gotoxy(10,1);
         lcd_puts("ws");
         lcd_puthex(webstatus);
         lcd_gotoxy(14,1);
         lcd_putc(' ');
         

         
         lcd_gotoxy(0,3);
         //lcd_putint(web_client_attempts);
         lcd_putc('d');
         lcd_putint12(sekundendiff);
         //lcd_putc(' ');

         loopcount0=0;
         // *** SPI senden
         //waitspi();
         //StartTransfer(loopcount1,1);
         
         lcd_gotoxy(10,1);
         lcd_puts("ws");
         lcd_puthex(webstatus);
         lcd_putc(' ');
          
         if (loopcount1 >= 0x08)
         {
            
            loopcount1 = 0;
            //OSZITOGG;
            LOOPLED_PORT ^= (1<<LOOPLED);
            //LOOPLEDPORT |= (1<<TWILED);           // TWILED setzen, Warnung
            //TWBR=0;
            //lcdinit();
         }
         else
         {
            loopcount1++;
         }
         
         
         if (LOOPLED_PIN &(1<<LOOPLED))
         {
            sec++; // zaehler fuer verzoegerung von Ping-aufnahme
         }
          
        // LOOPLED_PORT ^= (1<<LOOPLED);
      }
      
      //**	Beginn Current-Routinen	***********************
      
      
      //**    End Current-Routinen*************************
      
      
#pragma mark packetloop
      // handle ping and wait for a tcp packet
      plen=enc28j60PacketReceive(BUFFER_SIZE, buf);
      buf[BUFFER_SIZE]='\0'; // http is an ascii protocol. Make sure we have a string terminator.

   // fix IP, not used
      // DHCP renew IP:
 //     plen=packetloop_dhcp_renewhandler(buf,plen); // for this to work you have to call dhcp_6sec_tick() every 6 sec

      
      dat_p=packetloop_arp_icmp_tcp(buf,plen);
      if(plen==0)
      {
         
         // we are idle here. trigger arp and dns stuff here
         if (gw_arp_state==0)
         {
            //mk_net_str(str,gwip,4,'.',10);
            //lcd_clr_line(2);
            //lcd_puts(str);
            
            // find the mac address of the gateway (e.g your dsl router).
            get_mac_with_arp(gwip,TRANS_NUM_GWMAC,&arpresolver_result_callback);
            gw_arp_state=1;
         }
         
         if (get_mac_with_arp_wait()==0 && gw_arp_state==1)
         {
            // done we have the mac address of the GW
            gw_arp_state=2;
         }
         
         //dns_state=0;
         //gw_arp_state=2;
         
         if (dns_state==0 && gw_arp_state==2)
         {
            if (!enc28j60linkup())
            {
               linkuperr++;
               continue; // only for dnslkup_request we have to check if the link is up.
            }
            sec=0;
            dns_state=1;
            dnslkup_request(buf,WEBSERVER_VHOST,gwmac);
            continue;
         }
         
         if (dns_state==1 && dnslkup_haveanswer())
         {
            dns_state=2;
            dnslkup_get_ip(otherside_www_ip);
            
            //lcd_gotoxy(0,1);
            mk_net_str(str,otherside_www_ip,4,'.',10);
            //lcd_clr_line(1);
            //lcd_puts(str);
            //_delay_ms(1600);
            //lcd_clr_line(2);
            
         }
         if (dns_state!=2)
         {
            // retry every minute if dns-lookup failed:
            if (sec > 10)
            {
               dns_state=0;
            }
            // don't try to use web client before
            // we have a result of dns-lookup
            continue;
         }
         
         //----------
         //        if ((start_web_client==1)||(webstatus & (1<<CURRENTSEND))) // web_client in ping-callback gesetzt
         //        if (webstatus & (1<<CURRENTSEND)) // web_client in ping-callback gesetzt
         
         if (start_web_client==1)// start_web_client in ping-callback gesetzt
         {
            
            sec=0;
            start_web_client=2;
            //web_client_attempts++;
            
            
            // pingstring aufbauen
            mk_net_str(dhcpstr,pingsrcip,4,'.',10);
            urlencode(dhcpstr,urlvarstr);
            
            // ping callbackcount einsetzen
            // erstes data ohne &
            char key10[]="ping=";
            strcpy(pingstring,key10);
            char pingcountstring[4];
            itoa(ping_callback_count,pingcountstring,10);
            
            //lcd_gotoxy(4,0);
            //          lcd_putint(ping_callback_count);
            //         lcd_putc(' ');
            //          lcd_puts(pingcountstring);
            //          lcd_putc('*');
            char* pp =(char*)trimwhitespace(pingcountstring);
            
            strcat(pingstring,pp);
            
            
            char key3[]="&task=";
            strcat(pingstring,key3);
            uint8_t  task = 2;
            char taskstring[4];
            itoa(task,taskstring,10);
            char* taskstringsauber =(char*)trimwhitespace(taskstring);
            strcat(pingstring,taskstringsauber);
            
            
            char key11[]="&homeip=";
            strcat(pingstring,key11);
            strcat(pingstring,urlvarstr);
            
            
            mk_net_str(dhcpstr,otherside_www_ip,4,'.',10);
            char othersideipstr[20];
            urlencode(dhcpstr,othersideipstr);
            
            char key12[]="&othersideip=";
            strcat(pingstring,key12);
            char* otheripstr;
            otheripstr = trimwhitespace(othersideipstr);
            strcat(pingstring,otheripstr);
            
            char* pingstringsauber =trimwhitespace(pingstring);
            //lcd_puts(pingstringsauber);
            //lcd_putc('*');
            
            
            // pingstring aufbauen end
            
            client_browse_url((char*)PSTR("/cgi-bin/dhcp_ping.pl?"),pingstringsauber,PSTR(WEBSERVER_VHOST),&browserresult_callback,otherside_www_ip,gwmac);
            webstatus |= (1<<CALLBACKWAIT); // warten auf callback
            
            
            
            //           client_browse_url((char*)PSTR("/cgi-bin/dhcp.pl?"),CurrentDataString,PSTR(WEBSERVER_VHOST),&browserresult_callback,otherside_www_ip,gwmac);
            
            //            client_browse_url((char*)PSTR("/cgi-bin/dhcp.pl?data=13&x="),urlvarstr,PSTR(WEBSERVER_VHOST),&browserresult_callback,otherside_www_ip,gwmac);
            
            
            
         }
        
         // reset after a delay to prevent permanent bouncing
         if (sec>10 && start_web_client==2)
         {
    //        start_web_client=0;
            sec=0;
         }

         #pragma mark current-routinen 
         //
         // current-routinen
         if ((webstatus & (1<<CURRENTSEND)))// && (webstatus & (1<<DATAOK))  && (!(webstatus & (1<<CALLBACKWAIT))))
         {
            webstatus &= ~(1<<CURRENTSEND);
            webstatus &= ~(1<<DATAOK); // client_browse nur einmal
            lcd_gotoxy(12,1);
            //lcd_puts("ws");
            lcd_puthex(webstatus);
            lcd_putc('+');
            _delay_ms(50);
            web_client_attempts++;
            
            //lcd_clr_line(2);
            //OSZILO;
            
            // start_web_client=2;
            //strcat("pw=Pong&strom=360\0",(char*)teststring);
            
            //start_web_client=0; // ping wieder ermoeglichen
            
            // dhcp-string aufbauen
            mk_net_str(dhcpstr,pingsrcip,4,'.',10);
            urlencode(dhcpstr,urlvarstr);
            
            char key1[]="homeip=";
            strcpy(dhcpstring,key1);
            strcat(dhcpstring,urlvarstr);
            
            mk_net_str(dhcpstr,otherside_www_ip,4,'.',10);
            char otheripstr[20];
            urlencode(dhcpstr,otheripstr);
            
            char key2[]="&othersideip=";
            strcat(dhcpstring,key2);
            strcat(dhcpstring,otheripstr);
            
            
            
            char key3[]="&task=";
            strcat(dhcpstring,key3);
            uint8_t  task = 1;
            char taskstring[4];
            itoa(task,taskstring,10);
            char* taskstringsauber =(char*)trimwhitespace(taskstring);
            strcat(dhcpstring,taskstringsauber);
            
            /*
             char d[24];
             uint16_t zufall = rand() % 0x1F + 1;
             float randomleistung =leistung+zufall;
             dtostrf(randomleistung,10,2,d);
             char* dd=(char*)trimwhitespace(d); // whitespace weg
             strcat(dhcpstring,dd);
             */
            /*
             lcd_gotoxy(13,1);
             lcd_puts(dd);
             
             
             char key4[]="&mittel=";
             strcat(dhcpstring,key4);
             dtostrf(impulsmittelwert,10,2,d);
             char* mm=(char*)trimwhitespace(d); // whitespace weg
             strcat(dhcpstring,mm);
             */
            char key5[]="&counter=";
            strcat(dhcpstring,key5);
            
            char countstring[4];
            itoa(web_client_sendok,countstring,10);
            char* cc =(char*)trimwhitespace(countstring);
            strcat(dhcpstring,cc);
            
            char key6[]="&callback=";
            strcat(dhcpstring,key6);
            itoa(browser_callback_count,countstring,10);
            char* bb =(char*)trimwhitespace(countstring);
            strcat(dhcpstring,bb);
            
            //web_client_attempts
            char key7[]="&attempts=";
            strcat(dhcpstring,key7);
            itoa(web_client_attempts,countstring,10);
            char*aa =(char*)trimwhitespace(countstring);
            strcat(dhcpstring,aa);
            
            char key8[]="&senderr=";
            strcat(dhcpstring,key8);
            itoa(web_client_send_err,countstring,10);
            char*ee =(char*)trimwhitespace(countstring);
            strcat(dhcpstring,ee);
            
            // string putzen
            char* dhcpstringsauber = trimwhitespace(dhcpstring);
            uint8_t ld = strlen(dhcpstring);
            lcd_gotoxy(0,0);
            lcd_putc('d');
            lcd_putint(ld);
            
            uint8_t ls = strlen(dhcpstringsauber);
            lcd_putc('s');
            lcd_putint(ls);
          
            // dhcp-string aufbauen end
            
            // check
            
            lcd_gotoxy(16,3);
            lcd_puts("   \0");
            client_browse_url((char*)PSTR("/cgi-bin/dhcp.pl?"),dhcpstringsauber,PSTR(WEBSERVER_VHOST),&browserresult_callback,otherside_www_ip,gwmac);
            webstatus |= (1<<CALLBACKWAIT); // warten auf callback
            
            //_delay_ms(50);
            
            // Daten an strom.pl schicken
            
            //            client_browse_url((char*)PSTR("/cgi-bin/strom.pl?"),CurrentDataString,(char*)PSTR(WEBSERVER_VHOST),&strom_browserresult_callback);
            
            //           client_browse_url((char*)PSTR("/cgi-bin/strom.pl?"),CurrentDataString,PSTR(WEBSERVER_VHOST),&browserresult_callback,otherside_www_ip,gwmac);
            
            sendWebCount++;
            // lcd_gotoxy(0,3);
            // lcd_putint(sendWebCount);
            //webstatus &= ~(1<<DATAOK); // client_browse nur einmal
            //webstatus |= (1<<DATAPEND);
            
            //lcd_gotoxy(19,0);
            //lcd_putc('>');
            
            
            //OSZIHI;
         }

         //
         
         continue;
      }
      
      if(dat_p==0)
      { // plen!=0
         // check for incomming messages not processed
         // as part of packetloop_arp_icmp_tcp, e.g udp messages
         udp_client_check_for_dns_answer(buf,plen);
         //continue;
         
    
         
         
         continue;
      }
      
      
#pragma mark Server GET-Routinen
      
      
      if (strncmp("GET ",(char *)&(buf[dat_p]),4)!=0)
      {
         // head, post and other methods:
         //
         // for possible status codes see:
         // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
         dat_p=http200ok();
         dat_p=fill_tcp_data_p(buf,dat_p,PSTR("<h1>200 OK</h1>"));
         goto SENDTCP;
      }
      if (strncmp("/ ",(char *)&(buf[dat_p+4]),2)==0)
      {
         dat_p=http200ok();
         dat_p=print_webpage(buf);
         goto SENDTCP;
      }
      else
      {
         dat_p=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 401 Unauthorized\r\nContent-Type: text/html\r\n\r\n<h1>401 Unauthorized webserver https</h1>"));
         goto SENDTCP;
      }
      //
   SENDTCP:
      www_server_reply(buf,dat_p); // send data
      
   }
   return (0);
}
