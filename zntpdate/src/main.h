/**
 * \file main.h
 * \brief main header
 *
 * \author Jean-Michel Marino
 * \author Copyright (C) 2008 Jean-Michel Marino
 *
 * \note Options for source edition: tab = 2 spaces
 */

#ifndef MAIN_H_
#define MAIN_H_

#define ktHOSTNAMELEN 64         /*!< max host name len                          */

/*!
  \struct options_t
  \brief all options of zntpdate tool.
  ******************************************************************  
  */
typedef struct options_t {
  int m_verbose;                 /*!< verbose mode                               */
  int m_debug;                   /*!< debug mode                                 */
  int m_syslog;                  /*!< write log into syslog                      */
  int m_enableEST;               /*!< use European Summer Time to set date/time  */

  int m_version;                 /*!< NTP version (1,2 or 3 by default)          */
  float m_offset;                /*!< offset in seconds                          */  
  char m_host[ktHOSTNAMELEN+1];  /*!< NTP hostname or IP address                 */
  
}options_t;

#endif /* MAIN_H_ */
