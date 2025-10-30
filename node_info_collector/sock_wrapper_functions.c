#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "common.h"
#include "sock_wrapper_functions.h"

int wrapped_socket(int domain, int type, int protocol)
{
  int s = -1;
  if ((s = socket(domain, type, protocol)) < 0) {
    perror("[-]: socket()");
    // printf("%d\n", errno);
  }
  return s;
}

int wrapped_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen)
{
  int r = -1;
  if ((r = bind(sockfd, addr, addrlen)) < 0) {
    perror("[-]: bind()");
    // printf("%d\n", errno);
  }
  return r;
}

int wrapped_connect(int sock, struct sockaddr *addr, socklen_t addrlen)
{
  int r = -1;
  if ((r = connect(sock, addr, addrlen)) < 0) {
    perror("[-]: connect()");
    // printf("%d\n", errno);
  }
  return r;
}

int wrapped_listen(int sockfd, int backlog)
{
  int r = -1;
  if ((r = listen(sockfd, backlog)) < 0) {
    perror("[-]: listen()");
    // printf("%d\n", errno);
  }
  return r;
}

int wrapped_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
  int sock = -1;
  if((sock = accept(sockfd, addr, addrlen)) < 0) {
    perror("[-]: accept()");
    // printf("%d\n", errno);
  }
  return sock;
}

int wrapped_send(int sockfd, void *buf, size_t len, int flags)
{
  int r;
  if ((r = send(sockfd, buf, len, flags)) < 0) {
    perror("[-]: send()");
    // printf("%d\n",errno);
  }
  return r;
}

int wrapped_sendto(int sockfd, void *buf, size_t len, int flags,
                   struct sockaddr *dest_addr, socklen_t addrlen)
{
  int r;
  if ((r = sendto(sockfd, buf, len, flags, dest_addr, addrlen)) < 0) {
    perror("[-]: sendto()");
  }
  return r;
}

int wrapped_recv(int sockfd, void *buf, size_t len, int flags)
{
  int r;
  if ((r = recv(sockfd, buf, len, flags)) < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return IS_TIMEOUT;
    }
    perror("[-]: recv()");
  }
  return r;
}

int wrapped_read(int fd, void *buf, size_t count)
{
  int r = -1;
  if ((r = read(fd, buf, count)) < 0) {
    perror("[-]: read()");
    // printf("%d\n", errno);
  }
  return r;
}

int wrapped_write(int fd, void *buf, size_t count)
{
  int r = -1;
  if ((r = write(fd, buf, count)) < 0) {
    perror("[-]: write()");
    // printf("%d\n", errno);
  }
  return r;
}

int wrapped_recvfrom(int sockfd, void *buf, size_t len, int flags,
		     struct sockaddr *src_addr, socklen_t *addrlen)
{
  int r = -1;
  r = recvfrom(sockfd, buf, len, 0, src_addr, addrlen);
  if (r < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return IS_TIMEOUT;
    }
    perror("[-]: recvfrom()");
  }
  return r;
}

int wrapped_set_recv_timeout(int sock, int sec, int usec)
{
  struct timeval tv;
  tv.tv_sec = sec;
  tv.tv_usec = usec;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    perror("[-]: setsockopt(SO_RCVTIMEO)");
    return -1;
  }
  return 0;
}

int wrapped_epoll_create(int size)
{
  int epfd = -1;
  if ((epfd = epoll_create(size)) < 0) {
    perror("[-]: epoll_create()");
    // printf("%d\n", errno);
  }
  return epfd;
}

int wrapped_epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev)
{
  int r = -1;
  if ((r = epoll_ctl(epfd, op, fd, ev)) != 0) {
    perror("[-]: epoll_ctl()");
    // printf("%d\n", errno);
  }
  return r;
}

int wrapped_epoll_wait(int epfd, struct epoll_event *ev,
	       int max_events, int timeout)
{
  int numfds = -1;
  if ((numfds = epoll_wait(epfd, ev, max_events, timeout)) < 0) {
    perror("[-]: epoll_wait()");
    // printf("%d\n", errno);
  }
  return numfds;
}
