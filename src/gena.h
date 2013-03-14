/*
 * gena.h
 *
 *  Created on: 2013/03/13
 *      Author: tnakamoto
 */

#ifndef GENA_H_
#define GENA_H_

typedef struct GENA_INFO {
  char ip[16];
  int port;
  char xml[2048];
} GENA_INFO;

int create_gena_thread(pthread_t *thread, GENA_INFO *gena);

#endif /* GENA_H_ */
