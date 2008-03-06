/**
 * \file main.c
 * \brief Main program entry point
 *
 * \author Jean-Michel Marino
 * \author Copyright (C) 2008 Jean-Michel Marino
 *
 * \note Options for source edition: tab = 2 spaces
 */

/*
 *=====================================================================
 *
 * zntpdate: a program set date/time with offset
 *
 * Jean-Michel Marino
 *	mailto:coepark@free.fr
 *
 * Original page
 *	http://zntpdate.sourceforge.net
 *
 * License
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Adapted from ntpdate.c
 *  Copyright (C) 2003  Tim Hogard
 *
 * Original ntpdate.c:
 *  This code will query a ntp server for the local time and display
 *  it.  it is intended to show how to use a NTP server as a time
 *  source for a simple network connected device.
 *  This is the C version.  The orignal was in Perl
 *
 *  For better clock management see the offical NTP info at:
 *  http://www.eecis.udel.edu/~ntp/
 *
 *  written by Tim Hogard (thogard@abnormal.com)
 *  Thu Sep 26 13:35:41 EAST 2002
 *  Converted to C Fri Feb 21 21:42:49 EAST 2003
 *  this code is in the public domain.
 *  it can be found here http://www.abnormal.com/~thogard/ntp/
 *
 *======================================================================
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "main.h"
#include "ntpdate.h"
#include "trace.h"


/* -- global variables -- */

const char   *gAppVersion = "zntpdate v0.4.3";  /*!< Application version.   */

options_t    gAppOptions;                       /*!< Application options.   */
trace_desc_t *gAppTrace;                        /*!< Structure of trace     */

/* -- local variables -- */


/* -- local functions -- */

static void usage(void );
static void write_version(void);
static int  parse_cmd_line(int artgc, char **argv);


/**
 * \brief Main program entry point
 *****************************************************
 */
int main(int argc, char **argv)
{
  int err = 0;
  TraceType tt;

  /* -- set default options and parse arguments -- */
  if (argc <= 1) usage();

  /* parse command line arguments */
  err = parse_cmd_line(argc, argv);
  if(err) exit(err);

  /* init trace */
  gAppTrace = trace_init( gAppOptions.m_syslog ? eSyslog : eStdout);

  /* do ntpdate */
  err = ntpdate();
  //  if(err) exit(-1);

  /* close trace */
  trace_close( &gAppTrace);

  exit(0);
}

/**
 * \brief function to parse command line
 *****************************************************
 * 
 * \param argc number of arguments
 * \param argv argument's table
 * 
 * \return 0 if OK else <0
 */
static int parse_cmd_line(int argc, char **argv)
{
  int err = 0;
  int j = 0;
  char *p = NULL;
  char c = 0, *aaa = NULL;
  
  /* default */
  gAppOptions.m_version = 3; // NTP version 3
  
  /* parse the arguments */
  while( --argc > 0 ) {
    argv++;
    p = *argv;	
		
    /* interpret a flag with '-' */
    if( *p == '-' ) {
      if (p[1] == '-') {
        p += 2;
        if (--argc <= 0) {
          fprintf( stderr, "++++ No argument for '--'\n");
          err = -1; goto DONE;
        }
        argv++;
        continue;
      }
      
      while( *++p != '\0' ) {
        switch (c = *p) {
		case 'V': write_version(); exit(0); break;
		case 'h': usage(); break;
		case 'v': gAppOptions.m_verbose = 1; break;
		case 'd': gAppOptions.m_debug = 1; break;
		case 's': gAppOptions.m_syslog = 1; break;
		  
		  //---- flags with parameter..
		  //----
		case 'O':
		case 'o':
          {
            aaa = p + 1;
            if (*aaa == '\0') {
              aaa = *++argv;
              //if (--argc <= 0 || *aaa == '-') {
              if( --argc <= 0 ) {
                fprintf(stderr, "++++ Missing parameter after flag -%c\n", c);
                err = -2; goto DONE;
              }
            } else {
              while( p[1] != '\0' )
                p++;             
            }
			
            if( strchr("O", c) ) {
              for (j = 0; j < strlen(aaa); j++) {
                if (!strchr("+-0123456789.", aaa[j])) {
                  fprintf(stderr, "++++ Invalid parameter <%s> for flag -%c\n", aaa, c);
                  err = -3; goto DONE;
                }
              }
			}
			
			if( strchr("o", c)) {
			  for (j = 0; j < strlen(aaa); j++) {
                if (!strchr("12", aaa[j])) {
                  fprintf(stderr, "++++ Invalid parameter <%s> for flag -%c\n", aaa, c);
                  err = -4; goto DONE;
                }
              }
			}
			
			switch (c) {
			case 'o':
			  {	sscanf(aaa, "%d", &gAppOptions.m_version);
			  } break;	   
			case 'O':
			  { sscanf(aaa, "%f", &gAppOptions.m_offset);
			  } break;  
			}
            
          } break;
		  
		default:
		  fprintf(stderr, "++++ Unknown flag: -%c\n", c);
		  err = -5; goto DONE;
		  break;
		  
        } // switch (c = *p)
		
      } // while *++p != '\0'
	  continue;
    } // if( *p == '-' )
	
    
    if( strlen(p) == 0) {
      fprintf(stderr, "++++ IP address must be not null!\n");
	  err = -6; goto DONE;
    }
    else {
	  
      // check IP address validity
      /*
		for (j = 0; j < strlen(p); j++) {
        if( !strchr("0123456789.", p[j])) {
		fprintf(stderr, "++++ Invalid parameter '<%s>' for ip address\n", p);
		err = 2; goto DONE;
        }
		}
	  */
	  
      strncpy( gAppOptions.m_host, p, sizeof(gAppOptions.m_host));

      /*
		read_def_format();
		if (read_fmt_file(p, styd) < 0) {
		fprintf(stderr,
		"++++ Cannot open format file: %s\n",
		p);
		severity = 1;
		}
      */
      continue;
    }
	
  } // while  --argc > 0 
  
  if( strlen(gAppOptions.m_host) == 0) {
	fprintf(stderr, "No IP address specified\n");
	err = -7; goto DONE;
  }
  
DONE:
  if(err) fflush(stderr);
  return err;
}

/**
 * \brief display usage and exit
 *****************************************************
 * 
 */
static void usage(void)  
{
  write_version();
  fprintf( stdout,
		   "Set date and time with offset.\n"
		   "Usage: zndtpdate [options] host\n"
		   "where:\n"
		   " host         hostname or IP address of NTP server\n"
		   " options:\n"
		   "  .configuration:\n"
		   "     -o v     Specify the NTP version for outgoint packets as the integer version,  which\n"
           "              can  be 1 or 2. The default is 3. This allows ntpdate to be used with older\n"
           "              NTP versions.\n"
		   "     -O[+-]n  Offset to add before set date, indicate +/- value.\n"
		   "  .verbose/debug:\n"
		   "     -d       Enable the debugging mode, in which zntpdate will go\n"
		   "              through all the steps, but do not adjust the local clock.\n"
		   "     -s       Divert logging output from the standard output (default) to the system sys-\n"
           "              log facility. This is designed primarily for convenience of cron scripts.\n"
		   "     -v       Verbose mode. Information useful for\n"
		   "              general debugging will also be printed.\n"
		   "  .help/version:\n"
		   "     -h       Show this command summary.\n"
		   "     -V       Show program version.\n"
		   "Example:\n"
		   "  zntpdate -vd pool.ntp.org\n"
		   );
  exit(0);
}

/**
 * \brief write application version and copyright
 *****************************************************
 * 
 */
static void write_version(void)
{
  fprintf(stdout, "%s\n", gAppVersion);
  fprintf(stdout, "Written by Jean-Michel Marino (public.jmm@free.fr)\n");
}
