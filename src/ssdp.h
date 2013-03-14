/*
 * ssdp.h
 *
 *  Created on: 2013/03/11
 *      Author: tnakamoto
 */

#ifndef SSDP_H_
#define SSDP_H_

#include "gena.h"

typedef struct GENA_THREAD_INFO {
  pthread_t thread;
  GENA_INFO gena;
} GENA_THREAD_INFO;

int create_ssdp_thread();

#endif /* SSDP_H_ */
