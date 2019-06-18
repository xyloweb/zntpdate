/**
 * \file ntpdate.c
 * \brief ntpdate source
 *
 * \author Jean-Michel Marino
 * \author Copyright (C) 2008-2019 Jean-Michel Marino
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
 * 
 * See also : https://lettier.github.io/posts/2016-04-26-lets-make-a-ntp-client-in-c.html
 *=====================================================================
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>        /* for perror                     */
#include <sys/select.h>   /* for timeval struct             */
#include <time.h>         /* for mktime and struct tm       */
#include <sys/time.h>     /* for settimeofday function      */
#include <signal.h>       /* for sigaction()                */
#include <unistd.h>       /* for alarm function             */

#include "gettext.h"      /* for gettext functions          */
#define _(String) gettext (String)
#define N_(String) String

#include "main.h"
#include "trace.h"

#include "ntpdate.h"

/* -- other defines --*/
#define MAXLEN                    1024  /*!< check our buffers                       */
#define NTPMODETYPE                  3  /*!< NTP mode type client                    */
#define SUMMERTIMEMONTHBEGIN         3  /*!< European Summer Time begin at March     */
#define SUMMERTIMEMONTHEND          10  /*!< European Summer Time end at October     */
#define TIMEOUT_SECS                10  /*!< time for waiting response of NTP Server */
#define NTP_MAXREQUEST_TRIES         3  /*!< max retries to get response             */

#define NTP_TIMESTAMP_DELTA 2208988800  /*!< NTP time-stamp of 1 Jan 1970            */

/* -- GLOBALES -- */
extern options_t     gAppOptions;
extern trace_desc_t* gAppTrace;

/*!
  \struct ntp_packet
  \brief ntp packet structure
  ******************************************************************  
  */
typedef struct ntp_packet_t {  
  uint8_t li_vn_mode;       /*!<  8 bits. li, vn, and mode.                              
                                       li.  Two bits.   Leap indicator.                     
                                       vn.  Three bits. Version number of the protocol.     
                                     mode.  Three bits. Client will pick mode 3 for client. */

  uint8_t stratum;          /*!<  8 bits. Stratum level of the local clock.                 */
  uint8_t poll;             /*!<  8 bits. Maximum interval between successive messages.     */
  uint8_t precision;        /*!<  8 bits. Precision of the local clock.                     */

  uint32_t rootDelay;       /*!<  32 bits. Total round trip delay time.                     */
  uint32_t rootDispersion;  /*!<  32 bits. Max error aloud from primary clock source.       */
  uint32_t refId;           /*!<  32 bits. Reference clock identifier.                      */

  uint32_t refTm_s;         /*!<  32 bits. Reference time-stamp seconds.                    */
  uint32_t refTm_f;         /*!<  32 bits. Reference time-stamp fraction of a second.       */

  uint32_t origTm_s;        /*!<  32 bits. Originate time-stamp seconds.                    */
  uint32_t origTm_f;        /*!<  32 bits. Originate time-stamp fraction of a second.       */

  uint32_t rxTm_s;          /*!<  32 bits. Received time-stamp seconds.                     */
  uint32_t rxTm_f;          /*!<  32 bits. Received time-stamp fraction of a second.        */

  uint32_t txTm_s;          /*!<  32 bits. The most important field the client cares about. 
                                  Transmit time-stamp seconds.                              */
  uint32_t txTm_f;          /*!<  32 bits. Transmit time-stamp fraction of a second.        */

} ntp_packet_t;

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

int tries = 0;                     /*!< Count of times sent - GLOBAL for signal-handler access */
ntp_packet_t packet_sending = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
ntp_packet_t packet_receiving = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

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
  int    err=0;
  int    i;			                           // misc var i
  int    s = -1;                           // socket
  time_t tmit = -1;                        // the time -- This is a time_t sort of

  char hostname[ktHOSTNAMELEN+1];
  int  portno = 123;                       // NTP is port 123
  
  //unsigned char msg[NTP_MAXREQUEST_LEN];   // = { eNTP_V1,0,0,0,0,0,0,0,0};  // the packet we send
  //unsigned long buf[MAXLEN];               // the buffer we get back
  //unsigned      msgLen;


  
  struct hostent     *he = NULL;           // host for gethostbyname
  struct protoent    *proto = NULL;	       // proto
  struct sockaddr_in server_addr;          // the socket structure

  struct sigaction myAction;               // for setting signal handler

  /*
   *  get hostname options
   ***************************************************************************
   */
  strncpy( hostname, gAppOptions.m_host, sizeof(hostname));
  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_MSG_TYPE,
                 _("Try NTP with host: %s"), hostname);
  }
  
  /*
   * open UDP socket
   ***************************************************************************
   */
  proto = getprotobyname( "udp");
  if( (s = socket( PF_INET, SOCK_DGRAM, proto->p_proto)) < 0) {
    trace_write( gAppTrace, eERROR_MSG_TYPE, _("socket() failed"));
    goto BAIL;    
  }
  else {
    if( s && gAppOptions.m_verbose ) {
      trace_write( gAppTrace, eINFO_MSG_TYPE, _("Open socket: %d"),s);
    }
  }
  
  /*
   * get ip address and try get host by name
   ***************************************************************************
   */
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
  
  /*
   * port number
   ***************************************************************************
   */
  server_addr.sin_port = htons(portno);
  trace_write( gAppTrace, eINFO_MSG_TYPE,
               _("Try to connect to hostname: '%s' (%s)..."),
               hostname,
               inet_ntoa( server_addr.sin_addr));
  trace_flush( gAppTrace);
  
  /*
   * build a message.  Our message is all zeros except for a one in the
   * protocol version field
   * msg[] in binary is 00 001 000 00000000 
   * it should be a total of 48 bytes long
   ***************************************************************************
   */
  
  // send the data with initializing first byte (NTP version and mode type client)
  //memset( msg, 0, sizeof(msg));
  
  //TODO : à revoir pour le déclarer dynamiquement dans l'objet packet
  //msg[0] = (gAppOptions.m_version == 1) ? eNTP_V1 :
  //  (gAppOptions.m_version == 2) ? eNTP_V2 : eNTP_V3;
  //msg[0] += NTPMODETYPE;
  packet_sending.li_vn_mode = (gAppOptions.m_version == 1) ? eNTP_V1 :
    (gAppOptions.m_version == 2) ? eNTP_V2 : eNTP_V3;
  packet_sending.li_vn_mode += NTPMODETYPE;

  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_MSG_TYPE, _("NTP version: %d"), gAppOptions.m_version);
  }
  
  /*
   * Set signal handler for alarm signal
   ***************************************************************************
   */
  myAction.sa_handler = CatchAlarm;
  if (sigfillset(&myAction.sa_mask) < 0) {  /* block everything in handler */
    trace_write( gAppTrace, eERROR_MSG_TYPE, _("sigfillset() failed"));
    goto BAIL;
  }
  myAction.sa_flags = 0;
  
  if (sigaction(SIGALRM, &myAction, 0) < 0) {
    trace_write( gAppTrace, eERROR_MSG_TYPE, _("sigaction() failed for SIGALRM"));
     goto BAIL;
  }

  /*
   * send to NTP server
   ***************************************************************************
   */ 
 
  //memset( &packet_sending, 0, sizeof( ntp_packet_t ) );

  //TODO: use 
  // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.
  //*( ( char * ) &packet_sending + 0 ) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

  i = sendto( s, &packet_sending, sizeof( ntp_packet_t ), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if( i == -1) {
    err = errno;
    trace_write( gAppTrace, eERROR_MSG_TYPE, _("sendto() failed"));
    if( gAppOptions.m_verbose) {
      trace_write( gAppTrace, eERROR_MSG_TYPE, _("sendto(): [error %d]"), err);
      trace_write( gAppTrace, eERROR_MSG_TYPE, "sendto(): %s", strerror(err));
    }
    goto BAIL;
  }
  else {
    if( gAppOptions.m_verbose) {
      trace_write( gAppTrace, eINFO_MSG_TYPE, _("Connected to '%s'"), hostname);
    }
  }
 
    
  /*
   * get the data back with timeout
   ***************************************************************************
   */
  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_MSG_TYPE, _("Attempt receive with timeout %ds"), TIMEOUT_SECS);
    trace_flush( gAppTrace);
  }


 //memset(buf, 0, sizeof(buf));

  alarm( TIMEOUT_SECS);        // Set the timeout
  while( (i = recv( s, &packet_receiving, sizeof(ntp_packet_t), 0)) < 0) {
    if( errno == EINTR) {                     // Alarm went off 
      if( tries < NTP_MAXREQUEST_TRIES) {     // incremented by signal handler
        
        if( gAppOptions.m_verbose) {
          trace_write( gAppTrace, eINFO_MSG_TYPE, _("Timed out, %d more tries..."), NTP_MAXREQUEST_TRIES - tries);
          trace_flush( gAppTrace);
        }
        
        // trie to send NTP request
        if( sendto( s, &packet_sending, sizeof(ntp_packet_t), 0, (struct sockaddr *)&server_addr,
                    sizeof(server_addr)) != sizeof(ntp_packet_t)) {
          trace_write( gAppTrace, eERROR_MSG_TYPE, _("sendto() failed"));
        }
        
        alarm( TIMEOUT_SECS);
      } 
      else {
        trace_write( gAppTrace, eERROR_MSG_TYPE, _("No Response, %d tries"), NTP_MAXREQUEST_TRIES);
        goto BAIL;
      }
    } 
    else {
      trace_write( gAppTrace, eERROR_MSG_TYPE, _("recvfrom() failed"));
      goto BAIL;
    }
  }
  // recvfrom() got something --  cancel the timeout 
  alarm(0);

  if(gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, _("Cool, I had an response!"));
  }
  
  /*
   * We get 12 long words back in Network order
   ***************************************************************************
   */
  if( gAppOptions.m_verbose) {
    u_long id = ntohl(packet_receiving.refId);
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "NTP.RefID: '0x%x'", id);

    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "li_vn_mode", ntohl(packet_receiving.li_vn_mode));

    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "stratum", ntohl(packet_receiving.stratum));
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "poll", ntohl(packet_receiving.poll));
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "precision", ntohl(packet_receiving.precision));

    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "rootDelay", ntohl(packet_receiving.rootDelay));
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "rootDispersion", ntohl(packet_receiving.rootDispersion));

    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "refTm_s", ntohl(packet_receiving.refTm_s));
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "refTm_f", ntohl(packet_receiving.refTm_f));

    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "origTm_s", ntohl(packet_receiving.origTm_s));
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "origTm_f", ntohl(packet_receiving.origTm_f));

    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "rxTm_s", ntohl(packet_receiving.rxTm_s));
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "rxTm_f", ntohl(packet_receiving.rxTm_f));

    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "txTm_s", ntohl(packet_receiving.txTm_s));
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "%s: 0x%.8x", "txTm_f", ntohl(packet_receiving.rxTm_f));
  }

  /* 
   * These two fields contain the time-stamp seconds as the packet left the NTP server.
   * The number of seconds correspond to the seconds passed since 1900.
   * ntohl() converts the bit/byte order from the network's to host's "endianness".
   ***************************************************************************
   */

  packet_receiving.txTm_s = ntohl( packet_receiving.txTm_s ); // Time-stamp seconds.
  packet_receiving.txTm_f = ntohl( packet_receiving.txTm_f ); // Time-stamp fraction of a second.

  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, "NTP.TransmitTime: 0x%.8x (%ld)", packet_receiving.txTm_s, packet_receiving.txTm_s);
  }
  if( packet_receiving.txTm_s <= 0 ) {
    trace_write( gAppTrace, eERROR_MSG_TYPE, _("Invalid transmit time"));
    goto BAIL;
  }
  
  /*
   * Convert time to unix standard time NTP is number of seconds since 0000
   * UT on 1 January 1900 unix time is seconds since 0000 UT on 1 January
   * 1970 There has been a trend to add a 2 leap seconds every 3 years.
   * Leap seconds are only an issue the last second of the month in June and
   * December if you don't try to set the clock then it can be ignored but
   * this is importaint to people who coordinate times with GPS clock sources.
   ***************************************************************************
   */
  
  tmit = ( time_t ) ( packet_receiving.txTm_s - NTP_TIMESTAMP_DELTA );

  if( gAppOptions.m_verbose) {
    trace_write( gAppTrace, eINFO_IN_MSG_TYPE, _("UNIX time: %ld"), tmit);
  } 
  /* use unix library function to show me the local time (it takes care
   * of timezone issues for both north and south of the equator and places
   * that do Summer time/ Daylight savings time.
   */  
  trace_write( gAppTrace,  eINFO_IN_MSG_TYPE, _("Time (GMT0): %s"), zctime(&tmit));

  /*
   * add European Summer Time adjust if option -E is enabled
   * WARNING: we must do this check before set offset !
   ***************************************************************************
   */  
  if( gAppOptions.m_enableEST) {
    time_t begin, end;
    struct tm *ltime = localtime( &tmit);

    err =  EuropeanSummerTime( ltime->tm_year + 1900, &begin, &end);
    if(err) {
      trace_write( gAppTrace, eWARNING_MSG_TYPE, _("EuropeanSummerTime() failed: [error %d]"), err);      
    }
    else {
      if( gAppOptions.m_verbose) {
        char *tmp = NULL;
        
        tmp = ctime(&begin);
        //tmp[strlen(tmp)-1] = '\0';
        trace_write( gAppTrace, eINFO_MSG_TYPE, _("European Summer Time start at: %s"), tmp);
        
        tmp = ctime(&end);
        //tmp[strlen(tmp)-1] = '\0';
        trace_write( gAppTrace, eINFO_MSG_TYPE, _("European Summer Time end at  : %s"), tmp);
      }
      
      if( tmit >= begin && tmit < end) {
        trace_write( gAppTrace, eINFO_MSG_TYPE, _("EST is activated")); 
        tmit += 3600;
      }
    }
  }

  /*  
   * add offset before set time of day
   ***************************************************************************
   */
  trace_write( gAppTrace, eINFO_MSG_TYPE, "Offset: %f", gAppOptions.m_offset);
  tmit += (time_t)gAppOptions.m_offset;
 
  /*
   * calculate new time and delta
   ***************************************************************************
   */
  i = time(0);
  trace_write( gAppTrace,  eINFO_MSG_TYPE, _("Time (new) : %s"), zctime(&tmit));
  trace_write( gAppTrace,  eINFO_MSG_TYPE, _("System time is %d seconds off"),i-tmit);

  /*
   * set time of day if it's necessary
   ***************************************************************************
   */
  if( 0 == i-tmit) {
    trace_write(gAppTrace,  eINFO_MSG_TYPE, _("Set time of day is not necessary"));
  }
  else if( gAppOptions.m_debug ) {
    trace_write( gAppTrace, eWARNING_MSG_TYPE, _("DEBUG ON: no set time of day activated."));
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
      trace_write(gAppTrace,  eERROR_MSG_TYPE, _("Set time of day failed !"));	
      err = errno;
      if( gAppOptions.m_verbose) {
        trace_write( gAppTrace, eERROR_MSG_TYPE, _("settimeofday() failed, [error %d]: %s"),
                     err,
                     strerror(err));
      }
    } 
    else {
      trace_write(gAppTrace,  eINFO_MSG_TYPE, _("Set time of day OK"));
    }
    trace_flush(gAppTrace);
  }
  
BAIL:
  if( gAppOptions.m_verbose)
    trace_write(gAppTrace,  eINFO_MSG_TYPE, _("Close socket: %d"), s);
  close(s);
  return err;
}
