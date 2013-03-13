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
  int sock;
  struct sockaddr_in sockaddr;
  struct ip_mreq mreq;
  fd_set master_rfds;
  fd_set work_rfds;

  fprintf(stdout, "ssdp looper start!\n");

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket ssdp");
    pthread_exit(NULL );
  }

  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(1900);
  sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0) {
    perror("bind ssdp");
    close(sock);
    pthread_exit(NULL );
  }

  memset(&mreq, 0, sizeof(mreq));
  mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
  mreq.imr_interface.s_addr = INADDR_ANY;
  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq,
                 sizeof(mreq)) < 0) {
    perror("setsockopt ssdp");
    close(sock);
    pthread_exit(NULL );
  }

  FD_ZERO(&master_rfds);
  FD_SET(sock, &master_rfds);

  while (1) {
    memcpy(&work_rfds, &master_rfds, sizeof(master_rfds));

    if (select(sock + 1, &work_rfds, NULL, NULL, NULL ) < 0) {
      perror("select ssdp");
      close(sock);
      pthread_exit(NULL );
    }

    if (FD_ISSET( sock, &work_rfds )) {
      char rbuf[BUFSIZ];
      if (recvfrom(sock, rbuf, sizeof(rbuf), 0, NULL, NULL ) <= 0) {
        perror("recvfrom ssdp");
        close(sock);
        pthread_exit(NULL );
      }
      fprintf(stdout, "===============================\n");
      fprintf(stdout, "%s\n", rbuf);
    }
  } /* while (1) */

  close(sock);

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

