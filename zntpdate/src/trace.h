/**
 * \file trace.h
 * \brief trace header
 *
 * \author Jean-Michel Marino
 * \author Copyright (C) 2008 Jean-Michel Marino
 *
 * \note Options for source edition: tab = 2 spaces
 */


#ifndef TRACE_H_
#define TRACE_H_

#define ktLOGMESSMAXLEN 80

/*
 *---------------------------------------------
 */
typedef enum TraceType {
  eSyslog  = 100,
  eStdout  = 101,

}TraceType;

/*
 *---------------------------------------------
 */
typedef struct {
  TraceType m_type;    /*!< type of trace     */
  FILE      *m_file;   /*!< file to put trace */

}trace_desc_t;

/*
 * Function prototype
 *---------------------------------------------
 */
trace_desc_t* trace_init  ( TraceType tt);
void          trace_close ( trace_desc_t **logID);
void          trace_write ( trace_desc_t *logID,  const char *format, ...);
void          trace_flush ( trace_desc_t *logID);

#endif /* TRACE_H_ */
