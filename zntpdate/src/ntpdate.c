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

#include <errno.h>          // for perror
#include <sys/select.h>     // for timeval struct

#include "main.h"
#include "trace.h"

#define MAXLEN	    1024    // check our buffers
#define NTPMODETYPE 3       // NTP mode type client

/* -- GLOBALES -- */
extern options_t     gAppOptions;
extern trace_desc_t* gAppTrace;

enum ntp_version {
  eNTP_V1 = 010,            /*!< 00 001 000 binary = v1 */
  eNTP_V2 = 020,            /*!< 00 010 000 binary = v2 */
  eNTP_V3 = 030,            /*!< 00 011 000 binary = v3 */

};
static int NTP_MODE_TYPE = 003;

/*
  fonction principale ntpdate
*/
int ntpdate(void)
{
  int err=0;

  //char	*hostname="88.178.32.159";
  char  hostname[ktHOSTNAMELEN+1];
  int	portno = 123;		      // NTP is port 123
  int	i;			              // misc var i
  //unsigned char msg[48] = {010,0,0,0,0,0,0,0,0};	// the packet we send
  unsigned char msg[48]; // = { eNTP_V1,0,0,0,0,0,0,0,0};	// the packet we send
  unsigned long buf[MAXLEN];      // the buffer we get back

  struct hostent     *he = NULL;      // host for gethostbyname
  struct protoent    *proto = NULL;	  // proto
  struct sockaddr_in server_addr;     // the socket structure

  int	 s = -1;                       // socket
  time_t tmit = -1;                    // the time -- This is a time_t sort of


  // get options
  strncpy( hostname, gAppOptions.m_host, sizeof(hostname));
  if( gAppOptions.m_verbose)
	trace_write( gAppTrace,
				 "Host: %s", hostname);

  //use Socket;
  //
  //#we use the system call to open a UDP socket
  //socket(SOCKET, PF_INET, SOCK_DGRAM, getprotobyname("udp")) or die "socket: $!";
  proto = getprotobyname( "udp");
  s = socket( PF_INET, SOCK_DGRAM, proto->p_proto);
  if( s && gAppOptions.m_verbose ) {
  	//perror("asd");
	trace_write( gAppTrace,
				 "Socket [%d]",s);
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
	/*
	unsigned long a = 0;
	a = (unsigned long)((struct in_addr*)he->h_addr)->s_addr;
	*/
	memcpy( (char *) &server_addr.sin_addr, he->h_addr_list[0], he->h_length);
  }

  //argv[1] );
  //i   = inet_aton(hostname,&server_addr.sin_addr);
  server_addr.sin_port = htons(portno);
  if( gAppOptions.m_verbose ) {
	trace_write( gAppTrace,
				 "Try to connect to hostname: '%s' (%s)...",
				 hostname,
				 inet_ntoa( server_addr.sin_addr));
	trace_flush( gAppTrace);
  }

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
	trace_write( gAppTrace,
				 "NTP version: %d", gAppOptions.m_version);
  }
  
  i=sendto(s,msg,sizeof(msg),0,(struct sockaddr *)&server_addr,sizeof(server_addr));
  if(i == -1) {
	err = errno;
	if( gAppOptions.m_verbose) {
	  trace_write( gAppTrace,
				   "sendto: [error %d]", err);
	  trace_write( gAppTrace,
				   "sendto: %s", strerror(err));
	}
    goto BAIL;
  }
  else {
	if( gAppOptions.m_verbose) {
	  trace_write( gAppTrace,
				   "Connect to %s: OK", hostname);
	}
  }

  // get the data back
  if( gAppOptions.m_verbose) {
	trace_write( gAppTrace,
				 "Attempt receive...");
	trace_flush( gAppTrace);
  }

  i = recv( s, buf, sizeof(buf), 0);
  if( i == -1) {
	err = errno;
	if( gAppOptions.m_verbose) {
	  trace_write( gAppTrace,
				   "recvfr %d",i);
	  trace_flush( gAppTrace);
	  trace_write( gAppTrace,
				   "recvfr: %s", strerror(err));
	}
	goto BAIL;
  }
  
  //We get 12 long words back in Network order
  if( gAppOptions.m_verbose) {
	//	char refID[4+1];
	u_long id = ntohl(buf[3]);

	//memset( &refID, 0, sizeof(refID));
	//refID[0] = (char)((id & 0xFF000000) >> 24);
	//refID[1] = (char)((id & 0x00FF0000) >> 16);
	//refID[2] = (char)((id & 0x0000FF00) >> 8);
	//refID[3] = (char)(id & 0xFF0000FF);

	trace_write( gAppTrace, "NTP.RefID: '0x%x'", id);
	
	/*
	  for( i = 0 ; i < 12 ; i++)
	  trace_write( gAppTrace,
	  "%.2d\t%-8x\n", i, ntohl(buf[i]));
	*/

  }
  
  /*
   * The high word of transmit time is the 10th word we get back
   * tmit is the time in seconds not accounting for network delays which
   * should be way less than a second if this is a local NTP server
   */
  
  tmit = ntohl( (time_t)buf[10]);	//# get transmit time
  if( gAppOptions.m_verbose) {
	trace_write( gAppTrace,
				 "tmit(NTP)  = %d", tmit);
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
	trace_write( gAppTrace,
				 "tmit(unix) = %d", tmit);
  }
  
  /* use unix library function to show me the local time (it takes care
   * of timezone issues for both north and south of the equator and places
   * that do Summer time/ Daylight savings time.
   */
  
  /*  
   * add offset before set time of day
   */
  trace_write( gAppTrace,
			   ">>>> Offset: %f", gAppOptions.m_offset);
  tmit += gAppOptions.m_offset;
  trace_write( gAppTrace,
			   ">>>> Time: %s", ctime(&tmit));
  i = time(0);
  //printf("%d-%d=%d\n",i,tmit,i-tmit);
  /*
   * and compare time delta
   */  
  trace_write( gAppTrace,
			   "System time is %d seconds off",i-tmit);
  
  /*
   * set time of day
   */
  if( gAppOptions.m_debug ) {
	trace_write( gAppTrace,
				 ">>>> Debug on: no set time of day activated.");
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

	trace_write(gAppTrace, (err ? ">>>> Set time of day: KO" : "Set time of day: OK"));
	trace_flush(gAppTrace);
	
	if( err) {
	  err = errno;
	  if( gAppOptions.m_verbose) {
		trace_write( gAppTrace, "settimeofday: [error %d]: %s",
					 err,
					 strerror(err));
	  }
	} 
  }

BAIL:
  return err;
}
