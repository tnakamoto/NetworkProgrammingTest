/*
 ============================================================================
 Name        : socket.c
 Author      : tnakamoto
 Version     :
 Copyright   : Your copyright notice
 Description : test
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

int send_msearch() {
  int sock;
  struct sockaddr_in sendaddr;
  struct sockaddr_in recvaddr;
  struct sockaddr_in server;
  socklen_t sin_size;
  char sndbuf[] =
      "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n"
          "MAN: \"ssdp:discover\"\r\nMX: 1\r\nST: urn:schemas-upnp-org:device:MediaServer:1";
  char recvbuf[4096];
  char *recvbuf_split;
  char *location;
  char *ip;
  char *port;
  char *xml;
  int n;
  char tcp_send_buf[4096] = { 0 };
  char tcp_recv_buf[4096] = { 0 };
  /**
   * M-SEARCH
   */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    fprintf(stderr, "socket err\n");
    return -1;
  }
  sendaddr.sin_family = AF_INET;
  sendaddr.sin_port = htons(1900);
  sendaddr.sin_addr.s_addr = inet_addr("239.255.255.250");

  recvaddr.sin_family = AF_INET;
  recvaddr.sin_port = htons(59000);
  recvaddr.sin_addr.s_addr = INADDR_ANY;
  bind(sock, (struct sockaddr *) &recvaddr, sizeof(recvaddr));
  fprintf(stdout, "sendto start\n");
  if (sendto(sock, sndbuf, strlen(sndbuf), 0, (struct sockaddr *) &sendaddr,
             sizeof(sendaddr)) < 0) {
    fprintf(stderr, "sendto error\n");
    return -1;
  }
  fprintf(stdout, "sendto end\nrcvfrom start\n");
  memset(recvbuf, 0, sizeof(recvbuf));
  if (recvfrom(sock, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *) &recvaddr,
               &sin_size) < 0) {
    fprintf(stderr, "recvfrom error\n");
    return -1;
  }

  fprintf(stderr, "%s\n", recvbuf);

  // LOCATIONからIP,Port,XMLを取得
  for (recvbuf_split = strtok(recvbuf, "\r\n"); recvbuf_split != NULL ;
      recvbuf_split = strtok(NULL, "\r\n")) {
    location = strstr(recvbuf_split, "LOCATION: http://");
    if (location != NULL ) {
      fprintf(stderr, "location = %s\n", location);
      ip = strtok(location + strlen("LOCATION: http://"), ":");
      if (ip != NULL ) {
        port = strtok(NULL, "/");
        if (port != NULL ) {
          xml = strtok(NULL, "\n");
        }
      }
      fprintf(stderr, "ip = %s\n", ip);
      fprintf(stderr, "port = %s\n", port);
      fprintf(stderr, "xml = %s\n", xml);
      break;
    }
  }

  close(sock);

  /**
   * Description
   */
  /* (1)ソケットを生成 */
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    exit(1);
  }

  /* (2)サーバに接続 */
  memset((char *)&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(ip);
  server.sin_port = htons(atoi(port));

  if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
    perror("connect");
    close(sock);
    exit(1);
  }

  /* (3)サーバに文字列を送信 */
  strcat(tcp_send_buf, "GET /");
  strcat(tcp_send_buf, xml);
  strcat(tcp_send_buf, " HTTP/1.1\r\nHOST: ");
  strcat(tcp_send_buf, ip);
  strcat(tcp_send_buf, ":");
  strcat(tcp_send_buf, port);
  strcat(tcp_send_buf, "\r\n\r\n");
  fprintf(stderr, "\n%s\n", tcp_send_buf);
  n = send(sock, tcp_send_buf, strlen(tcp_send_buf), 0);
  if (n < 0) {
    perror("send");
    close(sock);
    exit(1);
  }

  /* (4)サーバから文字列を受信 */
  n = recv(sock, tcp_recv_buf, sizeof(tcp_recv_buf), 0);
  if (n < 0) {
    perror("recv");
    close(sock);
    exit(1);
  } else if (n == 0) {
    /* コネクションが切断された */
  }

  /* 表示 */
  tcp_recv_buf[n] = '\0';
  fprintf(stderr, "%s(recv from %s)\n\n", tcp_recv_buf, ip);

  close(sock);

  return 0;
}

/*
 * echoサーバ (TCP + select版)
 */

#define SERVER_PORT     1900 /* サーバのポート番号 */

#define LISTEN_NUM      10 /* リッスンキューの長さ */
#define CONNECT_NUM     10 /* 同時に接続可能なコネクション数 */
#define CONNECT_TIMEOUT 20 /* 無操作タイムアウト時間 */

#define BUFSIZE         2048  /* バッファのサイズ */

/* クライアント情報を保持する構造体 */
typedef struct CLIENT_INFO {
  int sock; /* ソケット番号 */
#define INVALID_SOCK -1

  char ipaddr[16]; /* IP アドレス  */
  int port; /* ポート番号   */

  time_t last_access; /* 最終アクセス時刻 */
} CLIENT_INFO;

CLIENT_INFO client_info[CONNECT_NUM];

/*----------------------------------------------------------------------------
 * タイトル：
 *   コネクション確立要求の受理
 *
 * 内容：
 *   引数のリッスンソケットで accept()を実行し、コネクション確立する。
 *   コネクションを確立できたら、クライアント情報を構造体に登録する。
 *
 * 引数：
 *   リッスンソケット
 *
 * 戻り値：
 *   新コネクションのソケットディスクリプタ
 *----------------------------------------------------------------------------*/
int accept_connection(int sock) {
  int i;
  int len;
  int new_sock;
  struct sockaddr_in client;

  /* 空いているクライアント情報構造体を探す */
  for (i = 0; i < CONNECT_NUM; i++) {
    if (client_info[i].sock == INVALID_SOCK)
      break;
  }

  /* 空きがない場合はエラーを返す */
  if (i >= CONNECT_NUM)
    return -1;

  /* コネクション確立要求を受理する */
  len = sizeof(client);
  new_sock = accept(sock, (struct sockaddr *) &client, &len);
  if (new_sock == -1) {
    perror("accept");
    exit(1);
  }

  /* クライアント情報構造体に値を設定 */
  client_info[i].sock = new_sock;
  strncpy(client_info[i].ipaddr, inet_ntoa(client.sin_addr),
          sizeof(client_info[i].ipaddr));
  /* IP アドレス */
  client_info[i].port = ntohs(client.sin_port); /* ポート番号 */
  time(&client_info[i].last_access); /* 最終アクセス時刻 */

  fprintf(stderr, "connected: ip=%s  port=%d  fd=%d\n", client_info[i].ipaddr,
          client_info[i].port, client_info[i].sock);

  return new_sock;
}

/*----------------------------------------------------------------------------
 * タイトル：
 *   コネクションの切断
 *
 * 内容：
 *   引数のクライアントIDのコネクションを切断し、
 *   クライアント情報を初期化する。
 *
 * 引数：
 *   クライアントID
 *
 * 戻り値：
 *   なし
 *----------------------------------------------------------------------------*/
void close_connection(int client_id) {
  /* コネクションを切断し、ソケットをクローズ */
  shutdown(client_info[client_id].sock, SHUT_WR);
  close(client_info[client_id].sock);

  /* クライアント情報を削除 */
  client_info[client_id].sock = INVALID_SOCK;
}

/*----------------------------------------------------------------------------
 * タイトル：
 *   echo 実行関数
 *
 * 内容：
 *   引数のクライアントIDに関連付けられたソケットから read() で文字列を
 *   読み込み、それをそのまま送信する。
 *
 * 引数：
 *   クライアントID
 *
 * 戻り値：
 *   受信サイズ
 *----------------------------------------------------------------------------*/
int recv_and_reply(int client_id) {
  int read_size;
  char buf[BUFSIZE];

  /* データ受信 */
#ifdef MODE_TCP
  read_size = recv(client_info[client_id].sock, buf, sizeof(buf) - 1, 0);
#else
//  read_size = recv(client_info[client_id].sock, buf, sizeof(buf) - 1, 0);
  read_size = recvfrom(client_info[client_id].sock, buf, sizeof(buf), 0, NULL,
                       NULL );
  fprintf(stdout, "[%s][%s][l.%d] %d\n", __FILE__, __FUNCTION__, __LINE__,
          read_size);
#endif

  if (read_size == 0 || read_size == -1) {
    /* コネクション切断 or エラーの場合 */
    fprintf(stderr, "disconnected: ip=%s  port=%d  fd=%d\n",
            client_info[client_id].ipaddr, client_info[client_id].port,
            client_info[client_id].sock);
  } else {
    /* 応答を送信する */
    buf[read_size] = '\0';
    fprintf(stdout, "recv: ip=%s  port=%d  fd=%d  --> %s",
            client_info[client_id].ipaddr, client_info[client_id].port,
            client_info[client_id].sock, buf);
    write(client_info[client_id].sock, buf, strlen(buf));
    time(&client_info[client_id].last_access);
  }
  return read_size;
}

/*----------------------------------------------------------------------------
 * タイトル：
 *   タイムアウト確認関数
 *
 * 内容：
 *   引数のクライアントの無操作タイムアウトの確認を行う。
 *
 * 引数：
 *   クライアントID
 *
 * 戻り値：
 *   0: 未タイムアウト
 *   1: タイムアウト
 *----------------------------------------------------------------------------*/
unsigned char is_timeout(int client_id) {
  time_t now; /* 現在時刻 */

  /* クライアント情報が空の場合はとばす */
  if (client_info[client_id].sock == INVALID_SOCK)
    return 0;

  time(&now);

  /* タイムアウトしていないか確認 */
  if (now - client_info[client_id].last_access > CONNECT_TIMEOUT) {
    fprintf(stderr, "timeout (%d) -- %s:%d\n", client_info[client_id].sock,
            client_info[client_id].ipaddr, client_info[client_id].port);

    return 1; /* タイムアウト */
  }

  return 0;
}

/*----------------------------------------------------------------------------
 * タイトル：
 *   メイン関数
 *----------------------------------------------------------------------------*/
int main(void) {
  struct sockaddr_in server;
  int ssdp_sock;
  int gena_sock;
  fd_set master_rfds;
  fd_set work_rfds;
  int sock_optval = 1;
  int i;
  int ret;
#ifndef MODE_TCP
  struct ip_mreq ssdp_mreq;
#endif

//  fprintf(stderr, "[%s][%s][l.%d]\n", __FILE__, __FUNCTION__, __LINE__);
  /* クライアント情報を初期化 */
  for (i = 0; i < CONNECT_NUM; i++) {
    client_info[i].sock = -1;
  }

#ifdef MODE_TCP
  /* リッスンソケットを作成 */
  gena_sock = socket(AF_INET, SOCK_STREAM, 0);
  client_info[1].sock = gena_sock;
  if (gena_sock < 0) {
    perror("socket gena_sock");
    exit(1);
  }
#else
  /* リッスンソケットを作成 */
  ssdp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  client_info[0].sock = ssdp_sock;
  if (ssdp_sock < 0) {
    perror("socket ssdp_sock");
    exit(1);
  }
#endif

#ifdef MODE_TCP
  /* ソケットオプション設定 */
  if ( setsockopt(gena_sock, SOL_SOCKET, SO_REUSEADDR,
          &sock_optval, sizeof(sock_optval)) < 0 ) {
    perror("setsockopt");
    exit(1);
  }
#endif

  /* ソケットと、ポート番号＆IPアドレスを関連付ける */
  server.sin_family = AF_INET;
  server.sin_port = htons(SERVER_PORT);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

#ifdef MODE_TCP
  if (bind(gena_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
    perror("bind gena");
    close(gena_sock);
    exit(1);
  }
#else
  if (bind(ssdp_sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
    perror("bind ssdp");
    close(ssdp_sock);
    exit(1);
  }
#endif

#ifndef MODE_TCP
  memset(&ssdp_mreq, 0, sizeof(ssdp_mreq));
  ssdp_mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
  ssdp_mreq.imr_interface.s_addr = INADDR_ANY;
  if (setsockopt(ssdp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &ssdp_mreq,
                 sizeof(ssdp_mreq)) < 0) {
    perror("setsockopt");
    exit(1);
  }
#endif

#ifdef MODE_TCP
  /* コネクション接続要求の受信キューの長さを設定 */
  if ( listen(gena_sock, LISTEN_NUM) < 0 ) {
    perror("listen");
    close(gena_sock);
    exit(1);
  }

  fprintf(stdout,"listen port number -> %d\n", SERVER_PORT);
#else
#endif

  /* fd_setの設定 */
  /* 監視するfd_setをゼロクリア */
  FD_ZERO(&master_rfds);
  /* ソケットを監視対象に追加 */
#ifdef MODE_TCP
  FD_SET(gena_sock, &master_rfds);
#else
  FD_SET(ssdp_sock, &master_rfds);
#endif

  /*
   * メインループ
   */
  while (1) {

    int i;
    time_t now_time;
#ifdef TIMEOUT
    struct timeval tv; /* select のタイムアウト時間 */

    /* タイムアウト時間を10秒に設定 */
    tv.tv_sec = 10;
    tv.tv_usec = 0;
#endif

    /* master_rfds を work_rfds にコピー */
    memcpy(&work_rfds, &master_rfds, sizeof(master_rfds));

    fprintf(stdout, "wait select. listen_sock = %d, client_info[0].sock = %d\n",
            ssdp_sock, client_info[0].sock);

    /*
     * ソケットを監視
     */
#ifdef TIMEOUT
    ret = select(FD_SETSIZE, &work_rfds, NULL, NULL, &tv);
#else
//    ret = select(ssdp_sock + 1, &work_rfds, NULL, NULL, NULL );
    ret = select(FD_SETSIZE, &work_rfds, NULL, NULL, NULL );
#endif

    if (ret < 0) {
      /*----- エラー -----*/
      perror("select");
      exit(1);
    }

    else if (ret == 0) {
      /*----- タイムアウト -----*/

      /* 各クライアントのタイムアウト確認 */
      for (i = 0; i < CONNECT_NUM; i++) {

        if (is_timeout(i) == 1) {
          /* ソケットを監視対象から削除し、クローズ */
          FD_CLR(client_info[i].sock, &master_rfds);
          close_connection(i);
        }

      } /* for() */

      continue;
    }

    else {
      /*----- 状態が変化したソケットあり -----*/

      /* 以下で状態が変化したソケットに対する処理を実施する */
    }

    /*
     * 状態が変化したソケットを確認する
     *   --- リッスンソケットの状態を確認
     */
    if (FD_ISSET( ssdp_sock, &work_rfds )) { /* 状態を確認 */

      int new_sock;

      fprintf(stdout, "state change: listen-socket(%d)\n", ssdp_sock);

#ifdef MODE_TCP
      /* コネクション確立要求を受理する */
      new_sock = accept_connection(ssdp_sock);
      if ( new_sock != -1 ) {
        /* 監視対象に新たなソケットを追加 */
        FD_SET(new_sock, &master_rfds);
      }
#endif
    }

    /*
     * 状態が変化したソケットを確認する
     *   --- コネクション確立済みソケットの状態を確認
     */
    for (i = 0; i < CONNECT_NUM; i++) {

      /* クライアント情報が空の場合はとばす */
      if (client_info[i].sock == INVALID_SOCK)
        continue;

      /* 状態を確認 */
      if (FD_ISSET( client_info[i].sock, &work_rfds )) {

        fprintf(stdout, "state change: socket(%d)\n", client_info[i].sock);

        /* echo処理 */
        ret = recv_and_reply(i);
        if (ret <= 0) {
          /* ソケットを監視対象から削除し、クローズ */
          FD_CLR(client_info[i].sock, &master_rfds);
          close_connection(i);
          fprintf(stderr, "close connection\n");
        }

      } /* if ( FD_ISSET( client_info[i].sock, &work_rfds ) ) */

    } /* for ( i = 0; i < CONNECT_NUM; i++ ) */

  } /* while (1) */

  close(ssdp_sock);

  return 0;
}
