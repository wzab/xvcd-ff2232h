/* This work, "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd) 
 * by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/). 
 * "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/) 
 * by Avnet and is used by Xilinx for XAPP1251.
 *
 *  Description : XAPP1251 Xilinx Virtual Cable Server for Linux
 *
 * Support for FT2232H has been added by Wojciech M. Zabolotny (wzab@ise.pw.edu.pl) basing on the 
 * https://github.com/barawn/xvcd-anita project.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include <sys/mman.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h> 
#include <pthread.h>

#include "ftdi_xvc_core.h"

#define VENDOR 0x0403
#define PRODUCT 0x6010
#define MAP_SIZE      0x10000

static int verbose = 0;
static int fasttck = 0;
static int digilent = 0;

static int sread(int fd, void *target, int len) {
  unsigned char *t = target;
  while (len) {
    int r = read(fd, t, len);
    if (r <= 0)
      return r;
    t += r;
    len -= r;
  }
  return 1;
}

int handle_data(int fd) {

  const char xvcInfo[] = "xvcServer_v1.0:32768\n"; 

  do {
    int len, nr_bytes;
    char cmd[16];
    unsigned char blen[4];
    unsigned char buffer[32768], result[16384];
    memset(cmd, 0, 16);
	  
    if (sread(fd, cmd, 2) != 1)
      return 1;
	  
    if (memcmp(cmd, "ge", 2) == 0) {
      if (sread(fd, cmd, 6) != 1)
	return 1;
      memcpy(result, xvcInfo, strlen(xvcInfo));
      if (write(fd, result, strlen(xvcInfo)) != strlen(xvcInfo)) {
	perror("write");
	return 1;
      }
      if (verbose) {
	printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
	printf("\t Replied with %s\n", xvcInfo);
      }
      break;
    } else if (memcmp(cmd, "se", 2) == 0) {
      if (sread(fd, cmd, 9) != 1)
	return 1;
      memcpy(result, cmd + 5, 4);
      if (write(fd, result, 4) != 4) {
	perror("write");
	return 1;
      }
      if (verbose) {
	printf("%u : Received command: 'settck'\n", (int)time(NULL));
	printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
      }
      break;
    } else if (memcmp(cmd, "sh", 2) == 0) {
      if (sread(fd, cmd, 4) != 1)
	return 1;
      if (verbose) {
	printf("%u : Received command: 'shift'\n", (int)time(NULL));
      }
    } else {

      fprintf(stderr, "invalid cmd '%s'\n", cmd);
      return 1;
    }
    /* Here we go only during the shift command */
    if (sread(fd, blen, 4) != 1) return 1;
    len = blen[0]+256*(blen[1]+256*(blen[2]+256*blen[3]));
    //fprintf(stderr,"%d\n",len);
    nr_bytes = (len + 7)/8;
    if (sread(fd, buffer, nr_bytes * 2) != 1) return 1;
    if (ftdi_xvc_shift_command(len, buffer, result)) return 1;
    if (write(fd,result, nr_bytes) != nr_bytes) {
      perror("write");
      return 1;
    }   
  } while (1);
  /* Note: Need to fix JTAG state updates, until then no exit is allowed */
  return 0;
}

int main(int argc, char **argv) {
  int i;
  int s;
  int c; 
  int fd_uio;
   
  struct sockaddr_in address;
   


  opterr = 0;

  while ((c = getopt(argc, argv, "vfd")) != -1)
    switch (c) {
    case 'v':
      verbose = 1;
      break;
	case 'f':
	  fasttck = 1;
	  break;
	case 'd':
	  digilent = 1;
	  break;
    case '?':
      fprintf(stderr, "usage: %s [-vfd]\n", *argv);
      fprintf(stderr, " -v\tverbose output\n");
      fprintf(stderr, " -f\tuse higher TCK frequency\n");
      fprintf(stderr, " -d\tpull up ADBUS7 pin for boards with Digilent Adept downloader\n");
      return 1;
    }
  ftdi_xvc_init(verbose);
  
  if (ftdi_xvc_open_device(VENDOR, PRODUCT) < 0) {
    return 1;
  }
  
  if (ftdi_xvc_init_mpsse(fasttck, digilent) < 0) 
    return 1;

  s = socket(AF_INET, SOCK_STREAM, 0);
               
  if (s < 0) {
    perror("socket");
    return 1;
  }

   
  i = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(2542);
  address.sin_family = AF_INET;

  if (bind(s, (struct sockaddr*) &address, sizeof(address)) < 0) {
    perror("bind");
    return 1;
  }

  if (listen(s, 1) < 0) {
    perror("listen");
    return 1;
  }

  fd_set conn;
  int maxfd = 0;

  FD_ZERO(&conn);
  FD_SET(s, &conn);

  maxfd = s;

  while (1) {
    fd_set read = conn, except = conn;
    int fd;

    if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
      perror("select");
      break;
    }

    for (fd = 0; fd <= maxfd; ++fd) {
      if (FD_ISSET(fd, &read)) {
	if (fd == s) {
	  int newfd;
	  socklen_t nsize = sizeof(address);

	  newfd = accept(s, (struct sockaddr*) &address, &nsize);

	  //               if (verbose)
	  printf("connection accepted - fd %d\n", newfd);
	  if (newfd < 0) {
	    perror("accept");
	  } else {
	    printf("setting TCP_NODELAY to 1\n");
	    int flag = 1;
	    int optResult = setsockopt(newfd,
				       IPPROTO_TCP,
				       TCP_NODELAY,
				       (char *)&flag,
				       sizeof(int));
	    if (optResult < 0)
	      perror("TCP_NODELAY error");
	    if (newfd > maxfd) {
	      maxfd = newfd;
	    }
	    FD_SET(newfd, &conn);
	  }
	}
	else if (handle_data(fd)) {

	  if (verbose)
	    printf("connection closed - fd %d\n", fd);
	  close(fd);
	  FD_CLR(fd, &conn);
	}
      }
      else if (FD_ISSET(fd, &except)) {
	if (verbose)
	  printf("connection aborted - fd %d\n", fd);
	close(fd);
	FD_CLR(fd, &conn);
	if (fd == s)
	  break;
      }
    }
  }
  return 0;
}
