/*
 * ssdp.c
 *
 *  Created on: 2013/03/11
 *      Author: tnakamoto
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dlna.h"

int send_msearch() {
  int sock;
  struct sockaddr_in sendaddr;
  char sendbuf[] =
      "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n"
          "MAN: \"ssdp:discover\"\r\nMX: 1\r\nST: urn:schemas-upnp-org:device:MediaServer:1";

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket");
    return DLNA_FAILURE;
  }

  sendaddr.sin_family = AF_INET;
  sendaddr.sin_port = htons(1900);
  sendaddr.sin_addr.s_addr = inet_addr("239.255.255.250");

  if (sendto(sock, sendbuf, strlen(sendbuf), 0, (struct sockaddr *) &sendaddr,
             sizeof(sendaddr)) < 0) {
    perror("sendto");
    close(sock);
    return DLNA_FAILURE;
  }

  close(sock);

  return DLNA_SUCCESS;
}

void *ssdp_looper() {
  fprintf(stdout, "ssdp looper start!\n");
  return (void *) NULL ;
}

int ssdp_init() {
  pthread_t thread;
  if (pthread_create(&thread, NULL, ssdp_looper, NULL ) != 0) {
    perror("pthread_create");
    return DLNA_FAILURE;
  }
  pthread_join(thread, NULL );
  return DLNA_SUCCESS;
}

