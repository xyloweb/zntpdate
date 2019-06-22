/**
 * \file main.c
 * \brief Main program entry point
 *
 * \author Jean-Michel Marino
 * \author Copyright (C) 2008-2019 Jean-Michel Marino
 *
 * \note Options for source edition: tab = 2 spaces
 */

/*
 *=====================================================================
 *
 * zntpdate: a program set date/time with offset
 *
 * Jean-Michel Marino
 *	mailto:public.jmm@gmail.com
 *
 * Original page
 *	https://github.com/xyloweb/zntpdate
 *
 * License
 *  MIT License
 *  Copyright (c) 2019 Jean-Michel MARINO
 * 
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
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

#include "gettext.h" /* for gettext functions */
#define _(String) gettext (String)
#define N_(String) String

#include "main.h"
#include "ntpdate.h"
#include "trace.h"

/* -- global variables -- */

const char   *gAppVersion = "zntpdate v0.4.16"; /*!< Application version.   */

options_t    gAppOptions;                       /*!< Application options.   */
trace_desc_t *gAppTrace;                        /*!< App log structure      */


/* -- local functions -- */

static void usage(void );
static void write_version(void);
static int  parse_cmd_line(int artgc, char **argv);


/**
 * \brief Main program entry point
 *****************************************************
 * 
 * \param argc number of argument
 * \param argv arguments list
 *
 * \return 0 if success else < 0
 */
int main(int argc, char **argv)
{
  int err = 0;
  TraceType tt;

  /* -- for localization --*/
#ifdef ENABLE_NLS
  setlocale( LC_ALL, "");
  bindtextdomain( PACKAGE, PACKAGE_LOCAL_DIR);
  textdomain( PACKAGE);
#endif

  /* -- set default options and parse arguments -- */
  if (argc <= 1) usage();

  /* parse command line arguments */
  err = parse_cmd_line(argc, argv);
  if(err) goto BAIL;

  /* init trace */
  gAppTrace = trace_init( gAppOptions.m_syslog ? eSyslog : eStdout);

  /* do ntpdate */
  err = ntpdate();
  if(err) goto BAIL;

  /* close trace */
  if(gAppTrace) trace_close( &gAppTrace);

BAIL:
  exit(err);
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
          fprintf( stderr, _("%s No argument for\n"), gLogSignature[eERROR_MSG_TYPE]);
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
        case 'E': gAppOptions.m_enableEST = 1; break;
          
          /* flags with parameter.. */
        case 'O':
        case 'o':
          {
            aaa = p + 1;
            if (*aaa == '\0') {
              aaa = *++argv;
              //if (--argc <= 0 || *aaa == '-') {
              if( --argc <= 0 ) {
                fprintf(stderr, _("%s Missing parameter after flag -%c\n"), gLogSignature[eERROR_MSG_TYPE], c);
                err = -2; goto DONE;
              }
            } else {
              while( p[1] != '\0' )
                p++;             
            }
            
            if( strchr("O", c) ) {
              for (j = 0; j < strlen(aaa); j++) {
                if (!strchr("+-0123456789.", aaa[j])) {
                  fprintf(stderr, _("%s Invalid parameter <%s> for flag -%c\n"), gLogSignature[eERROR_MSG_TYPE], aaa, c);
                  err = -3; goto DONE;
                }
              }
            }
            
            if( strchr("o", c)) {
              for (j = 0; j < strlen(aaa); j++) {
                if (!strchr("12", aaa[j])) {
                  fprintf(stderr, _("%s Invalid parameter <%s> for flag -%c\n"), gLogSignature[eERROR_MSG_TYPE], aaa, c);
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
          fprintf(stderr, _("%s Unknown flag: -%c\n"), gLogSignature[eERROR_MSG_TYPE], c);
          err = -5; goto DONE;
          break;
          
        } // switch (c = *p)
        
      } // while *++p != '\0'
      continue;
    } // if( *p == '-' )
    
    
    if( strlen(p) == 0) {
      fprintf(stderr, _("%s IP address must be not null!\n"), gLogSignature[eERROR_MSG_TYPE]);
      err = -6; goto DONE;
    }
    else { 
      strncpy( gAppOptions.m_host, p, sizeof(gAppOptions.m_host));
      
      continue;
    }
    
  } // while  --argc > 0 
  
  if( strlen(gAppOptions.m_host) == 0) {
    fprintf(stderr, _("%s No IP address specified\n"), gLogSignature[eERROR_MSG_TYPE]);
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
           _("This tool is like ntpdate but I added a feature to make an offset before set system date\n"
             "and time. It is particulary interesting when your system is configured without TIMEZONE\n"
             "and when you could not set nothing else but GMT0.\n"
             "If you are localized into an European Summer Time zone don't forget to set -E option so\n"
             "than one hour will be automatically added in summer.\n"
             "\n"
             "Usage: zndtpdate [options] host\n"
             "where:\n"
             " host         hostname or IP address of NTP server\n"
             " options:\n"
             "  .configuration:\n"
             "     -o v     Specify the NTP version for outgoint packets as the integer version,  which\n"
             "              can  be 1 or 2. The default is 3. This allows ntpdate to be used with older\n"
             "              NTP versions.\n"
             "     -O[+-]n  Offset to add before set date, indicate +/- value (seconds).\n"
             "     -E       Enable automatic correction for the summer time.\n"
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
             "\n"
             "Examples:\n"
             "     How to add automatically 1 hour in winter and 2 hours in summer time?\n"
             "              zntpdate -Ev -O+3600 pool.ntp.org\n"
             "     How to test znptdate without change date and time of your system?\n"
             "              zntpdate -dv pool.ntp.org\n"
             )
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
#ifdef HAVE_CONFIG_H
  printf( "%s\n", PACKAGE_STRING);
  printf( _("Written by Jean-Michel Marino\n"));
  printf( _("Report bugs to <%s>.\n"), PACKAGE_BUGREPORT);
#else
  printf( "%s\n", gAppVersion);
  printf( "%s\n", _("Written by Jean-Michel Marino (public.jmm@gmail.com)"));
#endif
}
