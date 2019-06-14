/**
 * \file trace.c
 * \brief Handle application log
 *
 * \author Jean-Michel Marino
 * \author Copyright (C) 2008-2009 Jean-Michel Marino
 *
 * \note Options for source edition: tab = 2 spaces
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <assert.h>
#include <syslog.h>
#include <time.h>    /* for function create_timestamp_msg */

#include "gettext.h" /* for gettext functions             */
#define _(String) gettext (String)
#define N_(String) String

#include "trace.h"

/* -- local functions -- */

/*!
  \brief "chug" the string (remove leading spaces and tab),
  and "chomp" it (avoid \n on last field).
  ******************************************************************
  *
  * \param string string to clean
  */
static void str_chug_chomp( char *string)
{
  char *p;
  size_t len;

  for( p = string; (*p == '\t' || *p == ' '); p++);
  len = strlen(p);
  if(len > 0) {
    for(; strchr( "\n", p[len]); len--);
  }

  memmove(string, p, len+1);
  string[len+1] = '\0';
}

/*!
  \brief create and time stamp to the string
  ******************************************************************
  *
  *\param logMess message to log
  *
  *\return message with timestamp
  */
static char *create_timestamp_msg( const char *logMess)
{
	char *tmp = NULL;
	time_t systime; 
	struct tm *utc = NULL;
 
    memset( &systime, 0, sizeof(systime));
	systime = time(NULL); 
	utc = localtime( &systime); 

	if( NULL != (tmp = (char *)calloc(ktLOGMESSMAXLEN + 1, sizeof(char)))) {
  	snprintf( tmp, ktLOGMESSMAXLEN, "%02d/%02d/%04d | %02d:%02d:%02d | %s",
              utc->tm_mday, (1+utc->tm_mon), (1900+utc->tm_year),
              utc->tm_hour, utc->tm_min, utc->tm_sec,
              logMess );
	}
	
	return tmp;
}
	
/*!
  \brief Init trace structure
  ****************************************************************** 
  
  This function is the first you must use to init trace
  structure for use trace module.
  
  \param tt the type of trace you want.
  \return new initialized structure or NULL if failed
*/
trace_desc_t *trace_init( TraceType tt)
{
  trace_desc_t *id = NULL;
  char *tmp = NULL;
  
  id = (trace_desc_t *)calloc( (size_t)1, sizeof(*id));
  if(NULL == id) {
    fprintf(stderr, "\n");
    fprintf(stderr, _("%s trace init failed"), gLogSignature[eERROR_MSG_TYPE]);
  }

  id->m_type = tt;
  switch(tt) {
  case eSyslog:
    { openlog("zntpdate", 0, LOG_USER);
    } break;
	
  case eStdout:
    {
      char *msg = "log started";
      id->m_file = stdout;
      tmp = create_timestamp_msg( msg);
      fprintf(id->m_file, "\n%s %s\n", gLogSignature[eINFO_MSG_TYPE],
              tmp ? tmp : msg);
      free(tmp);
    } break;
	
  default:
    { fprintf(stderr, "\n");
      fprintf(stderr, _("%s log type not implemented."), gLogSignature[eERROR_MSG_TYPE]);
    } break;
  }
  
  return id;
}


/*!
  \brief close trace struture
   ******************************************************************
 
   Use this function to close your trace structure
   
   \param logID address of your trace structure pointer 
*/
void trace_close( trace_desc_t **logID)
{
  TraceType tt = -1;
  char *tmp = NULL;

  assert( *logID);

  tt = (*logID)->m_type;
  switch(tt) {
  case eSyslog:
    { closelog();
    } break;
	
  case eStdout:
    {
      char *msg = "log end.";
      tmp = create_timestamp_msg( msg);    	
      fprintf((*logID)->m_file, "\n\n%s %s\n", gLogSignature[eINFO_MSG_TYPE],
              tmp ? tmp : msg);
      free(tmp);
      trace_flush( *logID);
    } break;
	
  default:
    { fprintf(stderr, "\n");
      fprintf(stderr, _("%s log type not implemented."), gLogSignature[eERROR_MSG_TYPE]);
    } break;
  }
  
  if(*logID) {
    free(*logID);
    *logID = NULL;
  }
}

/*!
  \brief Write a message into your trace
  ******************************************************************
  
  \param logID   trace structure pointeur
  \param msgType type of message \sa LogMsgType
  \param format  message format
  \param ...     the message 
*/
void trace_write( trace_desc_t *logID, LogMsgType msgType, const char *format, ...)
{
  TraceType tt = -1;
  char logMess[ktLOGMESSMAXLEN+1];
  va_list pa;

  va_start( pa, format);
  
  tt = logID->m_type;
  switch(tt) {
  case eSyslog:
    { vsyslog( LOG_INFO, format, pa);
    } break;
    
  case eStdout:
	{ 
      char *tmp = NULL;

      if(!logID->m_file) goto DONE;
	  vsprintf( logMess, format, pa);
      str_chug_chomp(logMess);

      // ajout de la date
      if( eWITH_TIMESTAMP & LOG_MSG_OPTION(msgType)) {
        tmp = create_timestamp_msg( logMess );
      }
      fprintf( logID->m_file, "\n%s %s", gLogSignature[LOG_MSG_TYPE(msgType)], tmp ? tmp : logMess);
      free(tmp);

    } break;   
	
  default:
    { fprintf(stderr, "\n");
      fprintf(stderr, _("%s log type not implemented."), gLogSignature[eERROR_MSG_TYPE] );
    } break;
  }
  
DONE:
  va_end(pa);
}

/*!
  \brief Flush trace
  ******************************************************************
  
  \param logID trace structure pointeur
*/
void trace_flush( trace_desc_t *logID)
{
  if(logID->m_type == eStdout) fflush(logID->m_file);
}
 
