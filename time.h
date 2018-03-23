/*  
  *  Copyright  Droids Corporation,  Microb  Technology,  Eirbot (2005)
  * 
  *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; either version 2 of the License, or
  *  (at your option) any later version.
  *
  *  This program is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *
  *  You should have received a copy of the GNU General Public License
  *  along with this program; if not, write to the Free Software
  *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
  *
  *  Revision : $Id: time.h,v 1.2 2005/02/15 13:01:14 zer0 Exp $
  *
  */
 
#ifndef TIME_H
#define TIME_H
 
#ifndef __TIME_UNSIGNED
#define __TIME_UNSIGNED 1
#endif
 
#if __TIME_UNSIGNED
 struct tm
 {
   unsigned char tm_sec;				/* Seconds.     [0-60]      */
   unsigned char tm_min;				/* Minutes.     [0-59]      */
   unsigned char tm_hour;				/* Hours.       [0-23]      */
   unsigned char tm_mday;				/* Day.         [1-31]      */
   unsigned char tm_mon;				/* Month.       [0-11]      */
   int tm_year;							/* Year since 1900          */
   unsigned char tm_wday;				/* Day of week. [0-6]       */
   int tm_yday;							/* Days in year.[0-365]     */
   unsigned char tm_isdst;				/* Daylight saving time     */
   unsigned char tm_hundredth;			/* not standard 1/100th sec */
 };

#else
 struct tm
 {
   int tm_sec;							/* Seconds.     [0-60]  */
   int tm_min;                   		/* Minutes.     [0-59]  */
   int tm_hour;							/* Hours.       [0-23]  */
   int tm_mday;							/* Day.         [1-31]  */
   int tm_mon;							/* Month.       [0-11]  */
   int tm_year;							/* Year since 1900      */
   int tm_wday;							/* Day of week. [0-6]   */
   int tm_yday;							/* Days in year.[0-365] */
   int tm_isdst;						/* Daylight saving time */
   char *tm_zone;						/* Abbreviated timezone */
 };

#endif
 
 typedef unsigned long time_t;
	time_t	time(time_t *t);
	struct tm *gmtime(time_t *timep);
	struct tm *localtime(time_t *timep);
	time_t mktime(struct tm *timeptr);
	char *asctime(struct tm *timeptr);
	char *ctime(time_t *timep);
 
 #endif 								/* TIME_H */
