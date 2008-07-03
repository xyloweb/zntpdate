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

#define ktLOGMESSMAXLEN 80    /*!< max message len into log           */
#define ktLOGSIGNMAXLEN 4     /*!< max sign len into message          */

/*!
  \enum TraceType
  \brief Output log type (stdout, syslog, ...)
  ******************************************************************  
*/
typedef enum TraceType {
  eSyslog  = 100,             /*!< log into system syslog             */
  eStdout  = 101,             /*!< log into terminal                  */

}TraceType;


#define ktLOG_MSG_TYPE_MASK	0x00FF		/*<! message type mask          */
/*! use this macro to retrieve log message type                       */
#define LOG_MSG_TYPE(x) ( ktLOG_MSG_TYPE_MASK & (x) )

#define ktLOG_MSG_OPT_MASK	0xFF00		/*<! option message mask        */
/*! use this macro to retrieve log message options                    */
#define LOG_MSG_OPTION(x) ( ktLOG_MSG_OPT_MASK & (x) )

/*!
  \enum LogMsgType
  \brief Message type table
  ******************************************************************

  \sa gLogSignature
*/
typedef enum LogMsgType {
  eINFO_MSG_TYPE			= 0,        /*!< info message type                               */
  eINFO_IN_MSG_TYPE,              /*!< info message associated with action send to NTP */
  eINFO_OUT_MSG_TYPE,             /*!< info message associated with action get to NTP  */
  eWARNING_MSG_TYPE,              /*!< warning message type                            */
  eERROR_MSG_TYPE,                /*!< error message type                              */
  
  // until            = 0xFF
  
  // Option for the type of message
  eWITH_TIMESTAMP     = 1 << 8,   /*!< to add timestamp to the message             */
  // next             = 1 << 9,
  // etc              = 1 << 10 until 15
}LogMsgType;

/*! must be completed at the same time that LogMsgType enum */
static const char *gLogSignature[ktLOGSIGNMAXLEN+1] = {
	"####",
	"<<<<",
	">>>>",
	"!!!!",
	"++++",	
};


/*!
  \struct trace_desc_t
  \brief structure of trace object
  ******************************************************************

*/
typedef struct trace_desc_t {
  TraceType m_type;                  /*!< type of trace     */
  FILE      *m_file;                 /*!< file to put trace */

}trace_desc_t;


/*
  Function prototype
  ******************************************************************
  */
trace_desc_t* trace_init  ( TraceType tt);
void          trace_close ( trace_desc_t **logID);
void          trace_write ( trace_desc_t *logID,  LogMsgType msgType, const char *format, ...);
void          trace_flush ( trace_desc_t *logID);

#endif /* TRACE_H_ */
