/**
 * \file trace.c
 * \brief Gestion des traces de l'application
 *
 * \author Jean-Michel Marino
 * \author Copyright (C) 2008 Jean-Michel Marino
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

#include "trace.h"
	
/*!
  \brief Init trace structure
 *****************************************************
 
 This function is the first you must use to init trace
 structure for use trace module.
 
 \param tt the type of trace you want.
 \return new initialized structure
 
 */
trace_desc_t *trace_init( TraceType tt)
{
  trace_desc_t *id = NULL;
  
  id = (trace_desc_t *)calloc( (size_t)1, sizeof(id));
  if(NULL == id) fprintf(stderr, "+++ trace init failed");
  
  id->m_type = tt;
  switch(tt) {
  case eSyslog:
    { openlog("zntpdate", 0, LOG_USER);
    } break;
	
  case eStdout:
    { id->m_file = stdout;
    } break;
	
  default:
    { fprintf(stderr, "+++ type not implemented");
    } break;
  }
  
  return id;
}


/*!
  \brief close trace struture
 *****************************************************
 
 Use this function to close your trace structure
 
 \param logID address of your trace structure pointer
 
 */
void trace_close( trace_desc_t **logID)
{
  TraceType tt = -1;

  assert( *logID);

  tt = (*logID)->m_type;
  switch(tt) {
  case eSyslog:
    { closelog();
    }	break;
	
  case eStdout:
    { fprintf((*logID)->m_file, "\n#### END ####\n");
      fflush((*logID)->m_file);
    } break;
	
  default:
    { fprintf(stderr, "\n+++ type not implemented");
    } break;
  }
  
  if(*logID) {
    free(*logID);
    *logID = NULL;
  }
}

/*!
  \brief Write a message into your trace
 *****************************************************
 
 \param logID trace structure pointeur
 \param format message format
 \param ... the message
 
 */
void trace_write( trace_desc_t *logID,  const char *format, ...)
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
	{ if(!logID->m_file) goto DONE;
	  vsprintf( logMess, format, pa);
      fprintf( logID->m_file, "\n%s", logMess);
    } break;   
	
  default:
    {  fprintf(stderr, "\n+++ type not implemented");
    } break;
  }
  
DONE:
  va_end(pa);
}

void trace_flush( trace_desc_t *logID)
{
  if(logID->m_type == eStdout) fflush(logID->m_file);
}
 
