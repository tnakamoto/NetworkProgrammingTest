/*
 * gena.c
 *
 *  Created on: 2013/03/13
 *      Author: tnakamoto
 */

#include <stdio.h>
#include <pthread.h>

#include "dlna.h"
#include "gena.h"

void *gena_looper(void *arg) {
  GENA_INFO *gena = (GENA_INFO *) arg;
  fprintf(stdout, "gena looper start! IP=%s, PORT=%d, XML=%s\n", gena->ip,
          gena->port, gena->xml);
  return (void *) NULL ;
}

int create_gena_thread(pthread_t *thread, GENA_INFO *gena) {
  if (pthread_create(thread, NULL, gena_looper, gena) != 0) {
    perror("pthread_create gena");
    return DLNA_FAILURE;
  }
  pthread_join(*thread, NULL );
  return DLNA_SUCCESS;
}

