/**
 * \file ntpdate.c
 * \brief ntpdate source
 *
 * \author Jean-Michel Marino
 * \author Copyright (C) 2008 Jean-Michel Marino
 *
 * \note Options for source edition: tab = 2 spaces
 */

/*
 *=====================================================================
 * This code will query a ntp server for the local time and display
 * it.  it is intended to show how to use a NTP server as a time
 * source for a simple network connected device.
 * This is the C version.  The orignal was in Perl
 *
 * For better clock management see the offical NTP info at:
 * http://www.eecis.udel.edu/~mills/ntp.html
 *
 * written by Tim Hogard (thogard@abnormal.com)
 * Thu Sep 26 13:35:41 EAST 2002
 * Converted to C Fri Feb 21 21:42:49 EAST 2003
 * this code is in the public domain.
 * it can be found here http://www.abnormal.com/~thogard/ntp/
 *=====================================================================
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>                 // for perror
#include <sys/select.h>            // for timeval struct
#include <time.h>                  // for mktime and struct tm
#include <signal.h>                // for sigaction()

#include "main.h"
#include "trace.h"

#define MAXLEN	             1024  /*!< check our buffers                        */
#define NTPMODETYPE             3  /*!< NTP mode type client                     */
#define SUMMERTIMEMONTHBEGIN    3  /*!< European Summer Time begin at March      */
#define SUMMERTIMEMONTHEND     10  /*!< European Summer Time end at October      */

#define TIMEOUT_SECS           10  /*!< time for waiting response of NTP Server  */
#define NTP_MAXREQUEST_LEN     48  /*!< longest string for NTP request           */
#define NTP_MAXREQUEST_TRIES    3  /*!< max retries to get response              */

/* -- GLOBALES -- */
extern options_t     gAppOptions;
extern trace_desc_t* gAppTrace;

/*!
  \enum ntp_version
  \brief many NTP protocol version (eNTP_V3 by default)
  ******************************************************************  
*/
typedef enum ntp_version {
  eNTP_V1 = 010,                   /*!< 00 001 000 binary = v1 */
  eNTP_V2 = 020,                   /*!< 00 010 000 binary = v2 */
  eNTP_V3 = 030,                   /*!< 00 011 000 binary = v3 */

}ntp_version;
static int NTP_MODE_TYPE = 003;


int tries = 0;                     /* Count of times sent - GLOBAL for signal-handler access */

/*!
  \brief Handler for SIGALRM
  ******************************************************************

  \param ignored not used
*/
static void CatchAlarm(int ignored)
{
  tries += 1;
}


/*!
  \brief like ctime but without a bug under SCO !
  ******************************************************************

  \param time the time to convert into string
  \return a C string containing the date and time information in a
  human-readable format.
*/
static char *zctime( const time_t *time)
{
  struct tm *ptm = NULL;
  ptm = gmtime(time);
  return ( ptm ? asctime(ptm) : "????" );  
}


/*!
  \brief  Calculates start date and end of the period of summer time.
   ******************************************************************

  \param year calculates for this year
  \param bebin the start date
  \param end the end date
  \return 0 if OK or <0 if failed

  \warning the year must be lesser than 2099 !
*/
static int EuropeanSummerTime( const int year, time_t *begin, time_t *end)
{  
  struct tm st;
  
  /* ready until 2099 ! */
  if(year > 2099) return -1;

  /* start of summer time */
  memset( &st, 0, sizeof(st));
  st.tm_hour = 1;                             // 0-23 (1 heure GMT0)
  st.tm_year = year - 1900;                   // years since 1900
  st.tm_mday = 31 - (5 * year / 4 + 4) % 7;   // 1-31
  st.tm_mon = SUMMERTIMEMONTHBEGIN - 1;       // 0-11
  *begin = mktime( &st);

  /* end of summer time */
  memset( &st, 0, sizeof(st));
  st.tm_hour = 1;                             // 0-23 (1 heure GMT0)
  st.tm_year = year - 1900;                   // years since 1900
  st.tm_mday = 31 - (5 * year / 4 + 1) % 7;
  st.tm_mon = SUMMERTIMEMONTHEND - 1;
  *end = mktime( &st);

  return 0;
}


/*!
  \brief main ntpdate function
   ******************************************************************
 
   Use this function to set date/time get by NTP protocol

   return 0 if OK or <0 if failed
*/
int ntpdate(void)
{
  int err=0;
  int	i;			                             // misc var i

  char  hostname[ktHOSTNAMELEN+1];
  int	portno = 123;		                     // NTP is port 123
  unsigned char msg[NTP_MAXREQUEST_LEN];   // = { eNTP_V1,0,0,0,0,0,0,0,0};  // the packet we send
  unsigned long buf[MAXLEN];               // the buffer we get back
  unsigned msgLen;
  
  struct hostent     *he = NULL;           // host for gethostbyname
  struct protoent    *proto = NULL;	       // proto
  struct sockaddr_in server_addr;          // the socket structure

  int	 s = -1;                             // socket
  time_t tmit = -1;                        // the time -- This is a time_t sort of
  
  struct sigaction myAction;               // for setting signal handler

  // get hostname options
  strncpy( hostname, gAppOptions.m_host, sizeof(hostname));
  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_MSG_TYPE,
                 "Try with host: %s", hostname);
  }

  /*
   * open UDP socket
   ***************************************************************************
   */
  proto = getprotobyname( "udp");
  if( (s = socket( PF_INET, SOCK_DGRAM, proto->p_proto)) < 0) {
     trace_write( gAppTrace, eERROR_MSG_TYPE, "socket() failed");
     goto BAIL;    
  }
  else {
    if( s && gAppOptions.m_verbose ) {
      trace_write( gAppTrace, eINFO_MSG_TYPE, "Socket number: %d",s);
    }
  }

  // get ip address

  memset( &server_addr, 0, sizeof( server_addr ));
  server_addr.sin_family=AF_INET;

  // try get host by name
  he = gethostbyname( hostname);
  if( !he ||
      he->h_addrtype != AF_INET ||
      (int) he->h_length > (int) sizeof(struct in_addr)) {
    server_addr.sin_addr.s_addr = inet_addr(hostname);
  }
  else {
    memcpy( (char *) &server_addr.sin_addr, he->h_addr_list[0], he->h_length);
  }
  
  // port number
  server_addr.sin_port = htons(portno);
  trace_write( gAppTrace, eINFO_MSG_TYPE,
               "Try to connect to hostname: '%s' (%s)...",
               hostname,
               inet_ntoa( server_addr.sin_addr));
  trace_flush( gAppTrace);
  
  /*
   * build a message.  Our message is all zeros except for a one in the
   * protocol version field
   * msg[] in binary is 00 001 000 00000000 
   * it should be a total of 48 bytes long
   */
  
  // send the data with initializing first byte (NTP version and mode type client)
  memset( msg, 0, sizeof(msg));
  msg[0] = (gAppOptions.m_version == 1) ? eNTP_V1 :
    (gAppOptions.m_version == 2) ? eNTP_V2 : eNTP_V3;
  msg[0] += NTPMODETYPE;
  
  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_MSG_TYPE, "NTP version: %d", gAppOptions.m_version);
  }
  
  /*
   * Set signal handler for alarm signal
   ***************************************************************************
   */
  myAction.sa_handler = CatchAlarm;
  if (sigfillset(&myAction.sa_mask) < 0) {  /* block everything in handler */
    trace_write( gAppTrace, eERROR_MSG_TYPE, "sigfillset() failed");
    goto BAIL;
  }
  myAction.sa_flags = 0;
  
  if (sigaction(SIGALRM, &myAction, 0) < 0) {
     trace_write( gAppTrace, eERROR_MSG_TYPE, "sigaction() failed for SIGALRM");
     goto BAIL;
  }

  /*
   * send to NTP server
   ***************************************************************************
   */ 
  msgLen = sizeof(msg); 
  i = sendto( s, msg, msgLen, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if( i == -1) {
    err = errno;
    trace_write( gAppTrace, eERROR_MSG_TYPE, "sendto() failed");
    if( gAppOptions.m_verbose) {
      trace_write( gAppTrace, eERROR_MSG_TYPE, "sendto(): [error %d]", err);
      trace_write( gAppTrace, eERROR_MSG_TYPE, "sendto(): %s", strerror(err));
    }
    goto BAIL;
  }
  else {
    if( gAppOptions.m_verbose) {
      trace_write( gAppTrace, eINFO_MSG_TYPE, "Connected to '%s'", hostname);
    }
  }
    
  /*
   * get the data back with timeout
   ***************************************************************************
   */
  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_MSG_TYPE, "Attempt receive with timeout %ds", TIMEOUT_SECS);
    trace_flush( gAppTrace);
  }

  alarm( TIMEOUT_SECS);        /* Set the timeout */
  while( (i = recv( s, buf, sizeof(buf), 0)) < 0) {
    if( errno == EINTR) {                     // Alarm went off 
      if( tries < NTP_MAXREQUEST_TRIES) {     // incremented by signal handler
        
        if( gAppOptions.m_verbose) {
          trace_write( gAppTrace, eINFO_MSG_TYPE, "Timed out, %d more tries...", NTP_MAXREQUEST_TRIES - tries);
          trace_flush( gAppTrace);
        }
        
        // trie to send NTP request
        if( sendto( s, msg, msgLen, 0, (struct sockaddr *)&server_addr,
                    sizeof(server_addr)) != msgLen) {
          trace_write( gAppTrace, eERROR_MSG_TYPE, "sendto() failed");
        }
        
        alarm( TIMEOUT_SECS);
      } 
      else {
        trace_write( gAppTrace, eERROR_MSG_TYPE, "No Response, %d tries", NTP_MAXREQUEST_TRIES);
        goto BAIL;
      }
    } 
    else {
      trace_write( gAppTrace, eERROR_MSG_TYPE, "recvfrom() failed");
      goto BAIL;
    }
  }
  
  
  /* recvfrom() got something --  cancel the timeout */
  alarm(0);
  if(gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "Cool, I had an response!");
  }
  
  //We get 12 long words back in Network order
  if( gAppOptions.m_verbose) {
    u_long id = ntohl(buf[3]);
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "NTP.RefID: '0x%x'", id);
    for( i = 0 ; i < 12 ; i++) {
      trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%.2d: 0x%.8x", i, ntohl(buf[i]));
    }	
  }
  
  /*
   * The high word of transmit time is the 10th word we get back
   * tmit is the time in seconds not accounting for network delays which
   * should be way less than a second if this is a local NTP server
   */
  
  tmit = ntohl( (time_t)buf[10]);	//# get transmit time
  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "NTP.TransmitTime: 0x%.8x (%ld)", tmit, tmit);
  }
  
  /*
   * Convert time to unix standard time NTP is number of seconds since 0000
   * UT on 1 January 1900 unix time is seconds since 0000 UT on 1 January
   * 1970 There has been a trend to add a 2 leap seconds every 3 years.
   * Leap seconds are only an issue the last second of the month in June and
   * December if you don't try to set the clock then it can be ignored but
   * this is importaint to people who coordinate times with GPS clock sources.
   */
  
  tmit -= 2208988800U;	
  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "UNIX time: %ld", tmit);
  }
  
  /* use unix library function to show me the local time (it takes care
   * of timezone issues for both north and south of the equator and places
   * that do Summer time/ Daylight savings time.
   */
  /*
    trace_write( gAppTrace,  eINFO_MSG_TYPE, "Time: %s", ctime(&tmit));
    !!! ctime don't work properly so I use gmtime() and asctime()
  */    
  trace_write( gAppTrace,  eINFO_IN_MSG_TYPE, "Time (GMT0): %s", zctime(&tmit));

  /*  
   * add offset before set time of day
   ***************************************************************************
   */
  trace_write( gAppTrace, eINFO_MSG_TYPE, "Offset: %f", gAppOptions.m_offset);
  tmit += gAppOptions.m_offset;

  /*
   * add European Summer Time adjust if option -E is enabled
   ***************************************************************************
   */  
  if( gAppOptions.m_enableEST) {
    time_t begin, end;
    struct tm *ltime = localtime( &tmit);

    err =  EuropeanSummerTime( ltime->tm_year + 1900, &begin, &end);
    if(err) {
      trace_write( gAppTrace, eWARNING_MSG_TYPE, "EuropeanSummerTime() failed: [error %d]", err);      
    }
    else {
      if( gAppOptions.m_verbose) {
        char *tmp = NULL;
        
        tmp = ctime(&begin);
        //tmp[strlen(tmp)-1] = '\0';
        trace_write( gAppTrace, eINFO_MSG_TYPE, "European Summer Time start at: %s", tmp);
        
        tmp = ctime(&end);
        //tmp[strlen(tmp)-1] = '\0';
        trace_write( gAppTrace, eINFO_MSG_TYPE, "European Summer Time end at  : %s", tmp);
      }
      
      if( tmit >= begin && tmit < end) {
        trace_write( gAppTrace, eINFO_MSG_TYPE, "EST is activated"); 
        tmit += 3600;
      }
    }
  }
  
  /*
   * calculate new time and delta
   ***************************************************************************
   */
  i = time(0);
  trace_write( gAppTrace,  eINFO_MSG_TYPE, "Time (new) : %s", zctime(&tmit));
  trace_write( gAppTrace,  eINFO_MSG_TYPE, "System time is %d seconds off",i-tmit);

  /*
   * set time of day
   ***************************************************************************
   */
  if( gAppOptions.m_debug ) {
	trace_write( gAppTrace, eWARNING_MSG_TYPE, "DEBUG ON: no set time of day activated.");
  }
  else {
	struct timeval new_timeval;

	memset(&new_timeval, 0, sizeof(new_timeval));
	new_timeval.tv_sec = tmit;

#ifdef SYSV_TIMEOFDAY 
	err = settimeofday( &new_timeval);
#else
	err = settimeofday( &new_timeval, 0);
#endif
  
	if( err) {
    trace_write(gAppTrace,  eERROR_MSG_TYPE, "Set time of day failed !");	
    err = errno;
	  if( gAppOptions.m_verbose) {
      trace_write( gAppTrace, eERROR_MSG_TYPE, "settimeofday() failed, [error %d]: %s",
                   err,
                   strerror(err));
	  }
	} 
  else {
    trace_write(gAppTrace,  eINFO_MSG_TYPE, "Set time of day OK");
  }
  trace_flush(gAppTrace);
  }
  
BAIL:
  if( gAppOptions.m_verbose)
    trace_write(gAppTrace,  eINFO_MSG_TYPE, "Close socket: %d", s);
  close(s);
  return err;
}
