/*
 * ssdp.c
 *
 *  Created on: 2013/03/11
 *      Author: tnakamoto
 */

#include <stdio.h>
#include <pthread.h>

#include "dlna.h"

void *ssdp_looper() {
  fprintf(stdout, "ssdp looper start!\n");
  return (void *) NULL;
}

int ssdp_init() {
  pthread_t thread;

  if(pthread_create(&thread, NULL, ssdp_looper, NULL) != 0 ) {
    perror("pthread_create");
    return DLNA_FAILURE;
  }

  pthread_join(thread, NULL);

  return DLNA_SUCCESS;
}

