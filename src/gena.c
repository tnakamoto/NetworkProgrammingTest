/*
 * gena.c
 *
 *  Created on: 2013/03/13
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
#include "gena.h"

void *gena_looper(void *arg) {
  int sock;
  struct sockaddr_in sendaddr;
  char sendbuf[2048];
  char recvbuf[2048];
  ssize_t recvbyte;
  int is_first = 1;
  int content_length;
  GENA_INFO *gena = (GENA_INFO *) arg;
  fprintf(stdout, "gena looper start! IP=%s, PORT=%d, XML=%s\n", gena->ip,
          gena->port, gena->xml);

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket description");
    return (void *) NULL ;
  }

  memset(&sendaddr, 0, sizeof(sendaddr));
  sendaddr.sin_family = AF_INET;
  sendaddr.sin_addr.s_addr = inet_addr(gena->ip);
  sendaddr.sin_port = htons(gena->port);

  if (connect(sock, (struct sockaddr *) &sendaddr, sizeof(sendaddr)) < 0) {
    perror("connect description");
    close(sock);
    return (void *) NULL ;
  }

  sprintf(
      sendbuf,
      "GET /%s HTTP/1.1\r\nHOST: %s:%d\r\n\r\n", gena->xml, gena->ip, gena->port);

  if (send(sock, sendbuf, strlen(sendbuf), 0) < 0) {
    perror("send description");
    close(sock);
    return (void *) NULL ;
  }

  do {
    recvbyte = recv(sock, recvbuf, sizeof(recvbuf), 0);
    if (recvbyte < 0) {
      perror("recv description");
      close(sock);
      return (void *) NULL ;
    } else if (recvbyte == 0) {
      /* コネクション切断 */
      break;
    } else {
      if (is_first) {
        is_first = 0;
        content_length = 3073;  // TODO: CONTENT-LENGTH をヘッダから取得する
      }
      content_length -= recvbyte;
    }
    /* 表示 */
    recvbuf[recvbyte] = '\0';
    fprintf(stdout, "%s", recvbuf);
  } while (content_length > 0);

  close(sock);
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

